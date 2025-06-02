//
// A Media player
//
//
// Circle - A C++ bare metal environment for Raspberry Pi
// Copyright (C) 2020-2021  R. Stange <rsta2@o2online.de>
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
#include "cdplayer.h"

#include <assert.h>
#include <circle/sched/scheduler.h>
#include <circle/string.h>
#include <circle/synchronize.h>
#include <circle/util.h>

LOGMODULE("cdplayer");

CCDPlayer *CCDPlayer::s_pThis = 0;

CCDPlayer::CCDPlayer(CSoundBaseDevice *pSound)
    : m_pSound(pSound)
{
    assert(m_pSound != 0);

    // I am the one and only!
    assert(s_pThis == 0);
    s_pThis = this;

    LOGNOTE("CD Player starting");
    SetName("cdplayer");
    Initialize();
}

boolean CCDPlayer::SetDevice(CDevice *pBinFileDevice) {
	LOGNOTE("CD Player setting device");
	state = STOP;
	address = 0;
	m_pBinFileDevice = pBinFileDevice; 
	return true;
}

boolean CCDPlayer::Initialize() {
    // configure sound device
    LOGNOTE("CD Player Initializing. Allocating queue size %d frames", BUFFER_SIZE);
    // TODO: set up the sound device in kernel.cpp
    if (!m_pSound->AllocateQueueFrames(BUFFER_SIZE)) {
                LOGERR("Cannot allocate sound queue");
                // TODO: handle error condition
    }
    m_pSound->SetWriteFormat(FORMAT, WRITE_CHANNELS);
    if (!m_pSound->Start()) {
	    LOGERR("Couldn't start the sound device");
    }
    unsigned int total_queue_size = m_pSound->GetQueueSizeFrames();
    boolean isActive = m_pSound->IsActive();
    LOGNOTE("CD Player Initializing. Allocated queue size %u frames. Player active %d", total_queue_size, isActive);

    return TRUE;
}

CCDPlayer::~CCDPlayer(void) {
    s_pThis = 0;
}

boolean CCDPlayer::Pause() {
    LOGNOTE("CD Player pausing");
    state = STOP;
    return true;
}

boolean CCDPlayer::Resume() {
    LOGNOTE("CD Player resuming");
    state = PLAY;
    return true;
}

boolean CCDPlayer::Seek(u32 lba) {
    // See to the new lba
    LOGNOTE("CD Player seeking to %u", lba);
    address = lba;
    state = SEEK;
    return true;
}

boolean CCDPlayer::Play(u32 lba, u32 num_blocks) {
    LOGNOTE("CD Player playing from %u for %u blocks", lba, num_blocks);
    // The PlayAudio SCSI command has some weird exceptions
    // for LBA addresses:-
    //
    // 00000000 do nothing. It's preferable that this method
    //          will not be called if an LBA of zero is
    //          encountered
    //
    // FFFFFFFF resume playing. It's preferable that the Resume
    //          method will be called instead of passing this
    //          value to this method

    if (lba == 0x00000000) {
        // do nothing
    } else if (lba == 0xFFFFFFFF) {
        // resume
        return this->Resume();
    } else {
        // play from new lba
        address = lba;
	end_address = address + num_blocks;
        state = SEEK_PLAY;
    }
    return true;
}

void CCDPlayer::Run(void) {
    unsigned int total_queue_size = m_pSound->GetQueueSizeFrames();
    LOGNOTE("CD Player Run Loop initializing. Queue Size is %d frames", total_queue_size);

    // Play loop
    while (true) {
        if (state == SEEK || state == SEEK_PLAY) {
            LOGNOTE("Seeking to %u", unsigned(address * SECTOR_SIZE));
            u64 offset = m_pBinFileDevice->Seek(unsigned(address * SECTOR_SIZE));
            if (offset != (u64)(-1)) {
		LOGNOTE("Seeking successful");
                if (state == SEEK_PLAY) {
                    LOGNOTE("Switching to PLAY mode");
                    state = PLAY;
		}
            } else {
                LOGERR("Error seeking");
                // TODO store error condition and return via dedicated method call
                state = STOP;
                break;
            }
        }

        while (state == PLAY) {
            // Get available queue size in stereo frames
            unsigned int available_queue_size = total_queue_size - m_pSound->GetQueueFramesAvail();

            // Determine how many *full CD sectors* can fit into this free space
            //    (1 CD sector = 588 stereo frames)
            unsigned int sectors_that_can_fit_in_queue = available_queue_size / FRAMES_PER_SECTOR;

            int bytes_to_read = SECTOR_SIZE * sectors_that_can_fit_in_queue;

            if (bytes_to_read) {
		LOGNOTE("Reading %u bytes", bytes_to_read);
                // Perform the single large read
                int readCount = m_pBinFileDevice->Read(m_FileChunk, bytes_to_read);
                LOGDBG("Read %d bytes", readCount);

                if (readCount < bytes_to_read) {
                    // Handle error: partial read
                    LOGERR("Partial read");
                    // TODO store error condition and return via dedicated method call
                    state = STOP;
                    break;
                }

		LOGNOTE("We are at %u", address);
                // Keep track of where we are
                address += (readCount / SECTOR_SIZE);

		// Should we stop?
		if (address >= end_address) {
			LOGNOTE("Finished playing");
			state = STOP;
			break;
		}

                // Write to sound device
		LOGNOTE("About to write %d bytes", readCount);
                int writeCount = m_pSound->Write(m_FileChunk, readCount);
                if (writeCount != readCount) {
                    LOGERR("Couldn't write to sound device");
                    // TODO store error condition and return via dedicated method call
                    state = STOP;
                    break;
                }
            }

            // Let other tasks have cpu time
	    CScheduler::Get()->Yield();
        }
	CScheduler::Get()->Yield();
    }
}
