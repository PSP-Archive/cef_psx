/*
	Custom Emulator Firmware
	Copyright (C) 2014, Total_Noob

	This program is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef __MAIN_H__
#define __MAIN_H__

#include <common.h>

#include "executable_patch.h"

#define DEBUG

#ifdef DEBUG

void logmsg(char *msg);

#define log(...) \
{ \
	char msg[256]; \
	sprintf(msg,__VA_ARGS__); \
	logmsg(msg); \
}

#else

#define log(...);

#endif

u32 lzf_decompress(const void *in_data, u32 in_len, void *out_data, u32 out_len);

SceModule2 *sceKernelFindModuleByName660(const char *modname);
int sceKernelCheckExecFile660(void *buf, SceLoadCoreExecFileInfo *execInfo);
int sceKernelProbeExecutableObject660(void *buf, SceLoadCoreExecFileInfo *execInfo);

#endif