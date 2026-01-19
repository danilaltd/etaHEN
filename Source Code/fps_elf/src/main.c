/* Copyright (C) 2025 LightningMods

This program is free software; you can redistribute it and/or modify it
under the terms of the GNU General Public License as published by the
Free Software Foundation; either version 3, or (at your option) any
later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; see the file COPYING. If not, see
<http://www.gnu.org/licenses/>.  */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include <sys/mman.h>
#include <sys/ptrace.h>
#include <sys/syscall.h>
#include <sys/sysctl.h>
#include <sys/wait.h>

#include <ps5/kernel.h>
#include <ps5/klog.h>
#include <ps5/mdbg.h>
#include <stdarg.h>
#include <pthread.h>

#include <stdint.h>
#include <stddef.h>

#include "../include/Common.h"
#include "../include/Common.h"

typedef struct {
   int32_t type;             // 0x00
   int32_t req_id;           // 0x04
   int32_t priority;         // 0x08
   int32_t msg_id;           // 0x0C
   int32_t target_id;        // 0x10
   int32_t user_id;          // 0x14
   int32_t unk1;             // 0x18
   int32_t unk2;             // 0x1C
   int32_t app_id;           // 0x20
   int32_t error_num;        // 0x24
   int32_t unk3;             // 0x28
   char use_icon_image_uri;  // 0x2C
   char message[1024];       // 0x2D
   char uri[1024];           // 0x42D
   char unkstr[1024];        // 0x82D
 } OrbisNotificationRequest; // Size = 0xC30

uint64_t sceKernelGetProcessTimeCounter(void);
uint64_t sceKernelGetProcessTimeCounterFrequency(void);
int32_t sceGnmSubmitAndFlipCommandBuffers(uint32_t count, void *dcbGpuAddrs[], uint32_t *dcbSizesInBytes, void *ccbGpuAddrs[], uint32_t *ccbSizesInBytes, uint32_t videoOutHandle, uint32_t displayBufferIndex, uint32_t flipMode, int64_t flipArg);
int sceKernelSendNotificationRequest(int userId, OrbisNotificationRequest *request, size_t requestSize, int flags);

HOOK_INIT(sceGnmSubmitAndFlipCommandBuffers);

uint64_t currentDelta = 0;
uint64_t frame_count = 0;
double tscTick = 0;

void printf_notification(const char* fmt, ...)
{
	OrbisNotificationRequest noti_buffer = {0};

	va_list args;
	va_start(args, fmt);
	int len = vsnprintf(noti_buffer.message, sizeof(noti_buffer.message), fmt, args);
	va_end(args);

	// these dont do anything currently
	// that or the structure has changed
	// lets just copy messages for now
	/*
	noti_buffer.type = 0;
	noti_buffer.unk3 = 0;
	noti_buffer.use_icon_image_uri = 0;
	noti_buffer.target_id = -1;
	*/
	// trim newline
	if (noti_buffer.message[len - 1] == '\n')
	{
		noti_buffer.message[len - 1] = '\0';
	}
	sceKernelSendNotificationRequest(0, (OrbisNotificationRequest*)&noti_buffer, sizeof(noti_buffer), 0);
}

void CalculateAndPrintFPS() {
	uint64_t current_time = sceKernelGetProcessTimeCounter();
	uint64_t delta = (current_time - currentDelta) / tscTick;
	
	frame_count++;

	if (delta >= 1.0) {
		double fps = frame_count / delta;
		// Send FPS
		printf_notification("FPS %.2f", fps);

		frame_count = 0;
		currentDelta = current_time;
	}
}


int32_t sceGnmSubmitAndFlipCommandBuffers_hook(uint32_t count, void *dcbGpuAddrs[], uint32_t *dcbSizesInBytes, void *ccbGpuAddrs[], uint32_t *ccbSizesInBytes, uint32_t videoOutHandle, uint32_t displayBufferIndex, uint32_t flipMode, int64_t flipArg)
{
    CalculateAndPrintFPS();
    return HOOK_CONTINUE(sceGnmSubmitAndFlipCommandBuffers,
                         int32_t(*)(uint32_t, void **, uint32_t *, void **, uint32_t *, uint32_t, uint32_t, uint32_t, int64_t),
                         count, dcbGpuAddrs, dcbSizesInBytes, ccbGpuAddrs, ccbSizesInBytes, videoOutHandle, displayBufferIndex, flipMode, flipArg);
}


int
main(void) {
    tscTick = (double)sceKernelGetProcessTimeCounterFrequency();

    // char buff[256];
    // while(sceKernelMprotect(&buff, sizeof(buff), PROT_READ | PROT_WRITE | PROT_EXEC) != 0) {
    //     klog_puts("sceKernelMprotect failed, retrying...");
    //     sleep(1);
    // }
    HOOK32(sceGnmSubmitAndFlipCommandBuffers);

    return 0;
}