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

static void *(* _memcpy)(void *dst, const void *src, int len) = (void *)0x8800D90C;
static int (* _strcmp)(const char *s1, const char *s2) = (void *)0x8800E398;

int (* sceKernelBootLoadFile)(BootFileInfo *info, int load_flag, void *a2, void *a3, void *t0);

int (* DecryptExecutable)(void *buf, int size, int *retSize);

RebootexConfig *rebootex_config = (RebootexConfig *)0x88FB0000;

void ClearCaches()
{
	static void (* _sceKernelIcacheInvalidateAll)() = (void *)0x88000E98;
	static void (* _sceKernelDcacheWritebackInvalidateAll)() = (void *)0x88000744;

	_sceKernelIcacheInvalidateAll();
	_sceKernelDcacheWritebackInvalidateAll();
}

int DecryptExecutablePatched(void *buf, int size, int *retSize)
{
	if(*(u16 *)((u32)buf + 0x150) == 0x8B1F)
	{
		*retSize = *(u32 *)((u32)buf + 0xB0);
		_memcpy(buf, (void *)((u32)buf + 0x150), *retSize);
		return 0;
	}

	return DecryptExecutable(buf, size, retSize);
}

int PatchLoadCore(int (* module_bootstart)(SceSize args, void *argp), void *argp)
{
	u32 text_addr = ((u32)module_bootstart) - 0xAF8;

	u32 i;
	for(i = 0; i < 0x8000; i += 4)
	{
		u32 addr = text_addr + i;

		/* Allow custom modules */
		if(_lw(addr) == 0xAE2D0048)
		{
			DecryptExecutable = (void *)K_EXTRACT_CALL(addr + 8);
			MAKE_CALL(addr + 8, DecryptExecutablePatched);
			break;
		}
	}

	ClearCaches();

	return module_bootstart(8, argp);
}

int sceKernelBootLoadFilePatched(BootFileInfo *info, int load_flag, void *a2, void *a3, void *t0)
{
	if(_strcmp(info->name, "pspbtcnf.bin") == 0)
	{
		_memcpy(info->name, "/kd/pspbtcnf.bin\0", 17);
	}

	sceKernelBootLoadFile(info, load_flag, a2, a3, t0);

	return 0; //always return 0 to allow boot with unsuccessfully loaded files
}

int _start(void *reboot_param, struct SceKernelLoadExecVSHParam *vsh_param, int api, int initial_rnd) __attribute__((section(".text.start")));
int _start(void *reboot_param, struct SceKernelLoadExecVSHParam *vsh_param, int api, int initial_rnd)
{
	u32 i;
	for(i = 0; i < 0x8000; i += 4)
	{
		u32 addr = 0x88600000 + i;

		/* Redirect pointer */
		if((_lw(addr) & 0xFFE0FFFF) == 0x3C008B00)
		{
			_sb(0xA0, addr);
			continue;
		}

		/* Patch ~PSP header check (enable non ~PSP config file) */
		if(_lw(addr) == 0xAFA60000 && _lw(addr + 4) == 0x2403FFFF)
		{
			_sh(0, addr + 4);
			continue;
		}

		/* Patch call to LoadCore module_bootstart */
		if(_lw(addr) == 0x00600008)
		{
			_sw(0x00602021, addr - 8); //move $a0, $v1
			MAKE_JUMP(addr, PatchLoadCore);
			continue;
		}

		if(_lw(addr) == 0xAFBF0000 && _lw(addr + 8) == 0x00000000)
		{
			sceKernelBootLoadFile = (void *)K_EXTRACT_CALL(addr + 4);
			MAKE_CALL(addr + 4, sceKernelBootLoadFilePatched);
			continue;
		}
	}

	ClearCaches();

	/* Call original function */
	static int (* sceReboot)(void *reboot_param, struct SceKernelLoadExecVSHParam *vsh_param, int api, int initial_rnd) = (void *)0x88600000;
	return sceReboot(reboot_param, vsh_param, api, initial_rnd);
}