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

		if(ValidUserAddress((void *)stub->libname) && ValidUserAddress(stub->nidtable) && ValidUserAddress(stub->stubtable))
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

u32 FindFunction(const char *szMod, const char *szLib, u32 nid)
{
	SceModule2 *pMod = _sceKernelFindModuleByName(szMod);
	if(!pMod) return 0;

	int i = 0;
	while(i < pMod->ent_size)
	{
		SceLibraryEntryTable *entry = (SceLibraryEntryTable *)(pMod->ent_top + i);

		if(entry->libname && _strcmp(entry->libname, szLib) == 0)
		{
			u32 *table = entry->entrytable;
			int total = entry->stubcount + entry->vstubcount;

			int j;
			for(j = 0; j < total; j++)
			{
				if(table[j] == nid)
				{
					return table[total + j];
				}
			}
		}

		i += (entry->len * 4);
	}

	return 0;
}