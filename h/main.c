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

#include <pspkernel.h>

int _strcmp(const char *s1, const char *s2)
{
	int val = 0;
	const u8 *u1, *u2;

	u1 = (u8 *)s1;
	u2 = (u8 *)s2;

	while(1)
	{
		if(*u1 != *u2)
		{
			val = (int) *u1 - (int) *u2;
			break;
		}

		if((*u1 == 0) && (*u2 == 0))
		{
			break;
		}

		u1++;
		u2++;
	}

	return val;
}

__attribute__((noinline)) int ValidUserAddress(void *addr)
{
	if((u32)addr >= 0x08800000 && (u32)addr < 0x0A000000) return 1;
	return 0;
}

u32 FindImport(const char *libname, u32 nid)
{
	u32 i;
	for(i = 0x08800000; i < 0x0A000000; i += 4)
	{
		SceLibraryStubTable *stub = (SceLibraryStubTable *)i;

		if((stub->libname != libname) && ValidUserAddress((void *)stub->libname) && ValidUserAddress(stub->nidtable) && ValidUserAddress(stub->stubtable))
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

/*
	Everything you have to adjust is the linkfile, 'path' and arguments for 'StartTNV'.
*/
void _start() __attribute__((section(".text.start")));
void _start()
{
	/* Find imports in RAM */
	SceUID (* _sceIoOpen)(const char *file, int flags, SceMode mode) = (void *)FindImport("IoFileMgrForUser", 0x109F50BC);
	int (* _sceIoRead)(SceUID fd, void *data, SceSize size) = (void *)FindImport("IoFileMgrForUser", 0x6A638D83);
	int (* _sceIoClose)(SceUID fd) = (void *)FindImport("IoFileMgrForUser", 0x810C4BC3);

	int (* _sprintf)(char *buf, const char *fmt, ...) = (void *)FindImport("scePaf", 0xA138A376);

	static char path[64];
	_sprintf(path, "ms0:/PSP/SAVEDATA/%s/TN.BIN", (char *)0x09E80400);

	/* Load TN binary into scratchpad */
	SceUID fd = _sceIoOpen(path, PSP_O_RDONLY, 0);
	_sceIoRead(fd, (void *)0x49E80000, 0x4000);
	_sceIoClose(fd);

	void (* StartTNV)(const char *path) = (void *)0x09E80000;
	StartTNV(path);
}