//
// syslogdaemon.h
//
// Circle - A C++ bare metal environment for Raspberry Pi
// Copyright (C) 2017  R. Stange <rsta2@o2online.de>
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
#ifndef _circle_net_syslogdaemon_h
#define _circle_net_syslogdaemon_h

#include <circle/logger.h>
#include <circle/machineinfo.h>
#include <circle/sched/synchronizationevent.h>
#include <circle/sched/task.h>
#include <circle/sound/hdmisoundbasedevice.h>
#include <circle/sound/i2ssoundbasedevice.h>
#include <circle/sound/pwmsoundbasedevice.h>
#include <circle/sound/usbsoundbasedevice.h>
#include <circle/time.h>
#include <circle/timer.h>
#include <circle/types.h>
#include <circle/util.h>
#include <fatfs/ff.h>
#include <linux/kernel.h>

#define SYSLOG_VERSION 1
#define SYSLOG_PORT 514
#define SECTOR_SIZE 2352
#define BATCH_SIZE 16
#define FRAMES_PER_SECTOR (SECTOR_SIZE / 4)  // bytes per stereo frame
#define BUFFER_SIZE (FRAMES_PER_SECTOR * BATCH_SIZE)

#define FORMAT SoundFormatSigned16
#define TYPE s16
#define TYPE_SIZE sizeof(s16)
#define FACTOR ((1 << 15) - 1)
#define NULL_LEVEL 0

class CCDPlayer : public CTask {
   public:
    CCDPlayer(const char *pSoundDevice);
    ~CCDPlayer(void);
    boolean Initialize();
    void Run(void);

    enum PlayState {
        STOP,
        SEEK,
        SEEK_PLAY,
        PLAY
    };

   private:
   private:
    CSynchronizationEvent m_Event;
    static CCDPlayer *s_pThis;
    CSoundBaseDevice *m_pSound;
    CDevice *m_BinFileDevice;
    u32 address;
    PlayState state;
    u8 *m_FileChunk = new (HEAP_LOW) u8[BUFFER_SIZE];
};

#endif
