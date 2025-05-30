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
#include "config.h"

LOGMODULE("cdplayer");

CCDPlayer *CCDPlayer::s_pThis = 0;

CCDPlayer::CCDPlayer(CSoundBaseDevice *pSound, CDevice *pBinFileDevice)
: m_pSound(pSound),
  m_pBinFile(pBinFileDevice)
{
    assert (m_pSound != 0);

    // I am the one and only!
    assert(s_pThis == 0);
    s_pThis = this;

    SetName("cdplayer");
    Initialize();
}

boolean CCDPlayer::Initialize() {
	// configure sound device
	LOGNOTE("Allocating queue size %d frames", BUFFER_SIZE);
	if (!m_pSound->AllocateQueueFrames(BUFFER_SIZE))
	{
		LOGERR()"Cannot allocate sound queue");
		//TODO: handle error condition
	}

	m_pSound->SetWriteFormat(FORMAT, WRITE_CHANNELS);

    return TRUE;
}

CCDPlayer::~CCDPlayer(void) {
    s_pThis = 0;
}

bool CCDPlayer::Pause() {
	state = STOP;
	return true;
}

bool CCDPlayer::Resume() {
	state = PLAY;
	return true;
}

bool CCDPlayer::Seek(u32 lba) {
	// See to the new lba
	address = lba;
	state = SEEK;
	return true;
}

bool CCDPlayer::Play(u32 lba) {

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
		state = SEEK_PLAY;
	}
	return true;
}

void CCDPlayer::Run(void) {
	u8 total_queue_size = m_pSound->GetQueueSizeFrames ();

	// Play loop
	while (true) {

		if (state == SEEK || state == SEEK_PLAY) {
			u64 offset = m_pBinFile->Seek(address * SECTOR_SIZE);
            if (offset != (u64)(-1)) {
				if (state == SEEK_PLAY)
					state = PLAY;
			} else {
				LOGERR("Error seeking");
				// TODO store error condition and return via dedicated method call
				state = STOP;
				break;
			}
		}

		while (state == PLAY) {

			// Get available queue size in stereo frames
			u32 available_queue_size = total_queue_size - m_pSound->GetQueueFramesAvail());

			// Determine how many *full CD sectors* can fit into this free space
            //    (1 CD sector = 588 stereo frames)
			u32 sectors_that_can_fit_in_queue = available_queue_size / FRAMES_PER_SECTOR;

			u32 bytes_to_read = SECTOR_SIZE * sectors_that_can_fit_in_queue;

			if (bytes_to_read) {
				// Perform the single large read
				int readCount = m_pBinFile->Read(m_FileChunk, bytes_to_read);
				MLOGDEBUG("UpdateRead", "Read %d bytes in batch", readCount);

				if (readCount < static_cast<int>(bytes_to_read)) {
					// Handle error: partial read
					LOGERR("Partial read");
					// TODO store error condition and return via dedicated method call
					state = STOP;
					break;
				}

				// Keep track of where we are
				address += (readCount / SECTOR_SIZE); 

				// Write to sound device
				int writeCount = m_pSound->Write(m_FileChunk, readCount);
				if (writeCount != readCount) {
					LOGERR("Couldn't write to sound device");
					// TODO store error condition and return via dedicated method call
					state = STOP;
					break;
				}
			}

			// Let other tasks have cpu time
			CScheduler::Current()->Yield();
		}
	}
}
