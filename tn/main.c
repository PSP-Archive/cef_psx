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

#include "rebootex.h"

static int (* _sprintf)(char *buf, const char *fmt, ...) = (void *)0x8800E1D4;

SceUID (* _sceIoOpen)(const char *file, int flags, SceMode mode);
int (* _sceIoRead)(SceUID fd, void *data, SceSize size);
int (* _sceIoClose)(SceUID fd);

SceModule2 *(*_sceKernelFindModuleByName)(const char *name);

int (* RunReboot)(u32 *params);
int (* DecodeKL4E)(void *dest, u32 size_dest, void *src, u32 size_src);

RebootexConfig rebootex_config;

#define FRAMESIZE  0x96000			//in byte

typedef struct
{
	u16 x; //0x0
	u16 y; //0x2
	u16 linesize; //0x4
	u16 height; //0x6
	u8 unk; //0x8
	u8 framebuffer_sel; //0x9
} VramInfo;

typedef struct
{
	VramInfo info_1; //0
	VramInfo info_2; //0xA
	u8 info_sel; //0x14
	//u32 *vmc_1; //0x1A4
	//u32 *vmc_2; //0x1A8
	//u32 error_code; //0x1AC
} PopsRegister; //0x1B0

void InitPopsVram()
{
	/*
		frame buffer size: 0x140000 (640*512*4)
	*/

	PopsRegister *reg = (PopsRegister *)0x49FE0000;
	reg->info_1.x = 0x1F6;
	reg->info_1.y = 0;
	reg->info_1.linesize = 640 * 4;
	reg->info_1.height = 240;
	reg->info_1.unk = 2;
	reg->info_1.framebuffer_sel = 0;
	reg->info_sel = 0;
}

/* convert RGB888 to RGB565 */
u16 ColorConvert(u32 color)
{
	u16 r, g, b;
	r = (u16)((color & 0xFF) >> 3);
	g = (u16)(((color >> 8) & 0xFF) >> 2);
	b = (u16)(((color >> 16) & 0xFF) >> 3);
	return (u16)(r | (g << 5) | (b << 11));
}

__attribute__((noinline)) void FlashScreen(u32 color)
{
	u16 color16 = ColorConvert(color);
	u32 vram = 0x490C0000;
	u32 vramend = vram + FRAMESIZE;
	while(vram < vramend) *(u16*)(vram += 2) = color16;
}

__attribute__((noinline)) void ErrorFlashScreen()
{
	FlashScreen(0x0000FF);
	_sw(0, 0);
}

int MakeFileList()
{
	/* Open package */
	char path[64];
	_sprintf(path, "%s/FLASH0.TN", rebootex_config.savedata_path);

	SceUID fd = _sceIoOpen(path, PSP_O_RDONLY, 0);
	if(fd < 0) return fd;

	/* Get n_custom_files */
	int n_custom_files = 0;
	_sceIoRead(fd, &n_custom_files, sizeof(int));

	/* Get n_files */
	int n_files = 0;
	FileList *orig_file_list = (FileList *)0x8B000000;
	while(orig_file_list[n_files].buffer != NULL) n_files++;

	/* Add custom files to file_list */
	FileList *file_list = (FileList *)0x8BA00000;
	_memcpy((void *)&file_list[n_custom_files], (void *)orig_file_list, n_files * sizeof(FileList));
	file_list[n_files + n_custom_files].buffer = NULL; //end

	u32 buffer_pointer = (u32)&file_list[n_files + n_custom_files + 1];
	u32 path_pointer = 0x8BE00000;

	int res = 0;
	do
	{
		TNPackage package;
		res = _sceIoRead(fd, &package, sizeof(TNPackage));

		if(res > 0)
		{
			if(package.magic == 0x4B504E54)
			{
				/* Add path */
				file_list->name = (void *)path_pointer;
				_sceIoRead(fd, file_list->name, package.path_len);

				/* Add buffer */
				buffer_pointer = ALIGN(buffer_pointer, 0x40);
				file_list->buffer = (void *)buffer_pointer;
				_sceIoRead(fd, (void *)0x8BC00000, package.size);
				int uncompressed_size = lzf_decompress((void *)0x8BC00000, package.size, file_list->buffer, 1 * 1024 * 1024);

				/* Add size */
				file_list->size = uncompressed_size;

				/* Next entry */
				path_pointer += package.path_len;
				buffer_pointer += uncompressed_size;
				file_list++;
			}
		}
	} while(res > 0);

	_sceIoClose(fd);

	return res;
}

int RunRebootPatched(u32 *params)
{
	MakeFileList();
	return RunReboot(params);
}

int DecodeKL4EPatched(void *dest, u32 size_dest, void *src, u32 size_src)
{
	static int (* _sceKernelGzipDecompress)(u8 *dest, u32 destSize, const u8 *src, void **next) = (void *)0x8800F804;
	rebootex_config.rebootex_size = _sceKernelGzipDecompress((void *)0x88FC0000, 0x4000, rebootex, NULL);
	_memcpy((void *)0x88FB0000, &rebootex_config, sizeof(RebootexConfig));
	return DecodeKL4E(dest, size_dest, src, size_src);
}

void PatchLoadExec(u32 text_addr, u32 text_size)
{
	/* Allow loadexec in whatever user level. Ignore K1 Check */
	_sh(0x1000, text_addr + 0x16A6);
	_sh(0x1000, text_addr + 0x241E);
	_sh(0x1000, text_addr + 0x2622);

	int i;
	for(i = 0; i < text_size; i += 4)
	{
		u32 addr = text_addr + i;

		/* Patch to do things before reboot */
		if(_lw(addr) == 0x02202021 && _lw(addr + 4) == 0x00401821)
		{
			RunReboot = (void *)K_EXTRACT_CALL(addr - 4);
			MAKE_CALL(addr - 4, RunRebootPatched);
			continue;
		}

		if(_lw(addr) == 0x17C001D3) //0x2B7C
		{
			/* Ignore kermit calls */
//			_sw(0, addr);

			/* Redirect pointer to 0x88FC0000 */
			DecodeKL4E = (void *)text_addr + 0;
			MAKE_CALL(addr + (0x2DE0 - 0x2B7C), DecodeKL4EPatched);
			_sb(0xFC, addr + (0x2E2C - 0x2B7C));

			/* Allow loadexec in whatever user level. Make sceKernelGetUserLevel return 4 */
			MAKE_DUMMY_FUNCTION(addr + (0x3688 - 0x2B7C), 4);

			continue;
		}
	}
}

void kernel_function()
{
	/* Set k1 */
	asm("move $k1, $0\n");

	/* Repair sysmem */
	RepairSysmem();

	/* Find important function */
	u32 i;
	for(i = 0x88000000; i < (0x88400000 - 0x54 - 4); i += 4)
	{
		if (_lw(i+0x00) == 0x27BDFFE0 && _lw(i+0x04) == 0xAFB40010 &&
			_lw(i+0x08) == 0xAFB3000C && _lw(i+0x0C) == 0xAFB20008 &&
			_lw(i+0x10) == 0x00009021 && _lw(i+0x14) == 0x02409821 &&
			_lw(i+0x54) == 0x0263202A)
		{
			_sceKernelFindModuleByName = (void *)i;
			break;
		}
	}

	/* Find functions */
	_sceIoOpen = (void *)FindFunction("sceIOFileManager", "IoFileMgrForUser", 0x109F50BC);
	_sceIoRead = (void *)FindFunction("sceIOFileManager", "IoFileMgrForUser", 0x6A638D83);
	_sceIoClose = (void *)FindFunction("sceIOFileManager", "IoFileMgrForUser", 0x810C4BC3);

	/* Patch loadexec */
	SceModule2 *mod = _sceKernelFindModuleByName("sceLoadExec");
	u32 text_addr = mod->text_addr;

	PatchLoadExec(text_addr, mod->text_size);

	static void (* _sceKernelIcacheInvalidateAll)() = (void *)0x88000E98;
	static void (* _sceKernelDcacheWritebackInvalidateAll)() = (void *)0x88000744;

	_sceKernelIcacheInvalidateAll();
	_sceKernelDcacheWritebackInvalidateAll();

	/* Load menu */
	char path[64];
	_sprintf(path, "%s/MENU.PBP", rebootex_config.savedata_path);

	struct SceKernelLoadExecVSHParam param;

	_memset(&param, 0, sizeof(param));
	param.size = sizeof(param);
	param.argp = path;
	param.args = _strlen(param.argp) + 1;
	param.key = "game";

	int (* sceKernelLoadExecVSHMs2)(const char *file, struct SceKernelLoadExecVSHParam *param) = (void *)text_addr + 0x1DAC;
	sceKernelLoadExecVSHMs2(param.argp, &param);
}

void _start(const char *path) __attribute__((section(".text.start")));
void _start(const char *path)
{
	/* Terminate threads */
	int (*_sceKernelTerminateDeleteThread)(SceUID) = (void *)FindImport("ThreadManForUser", 0x383F7BCC);
	_sceKernelTerminateDeleteThread(*(SceUID *)0x09FFD800);
	_sceKernelTerminateDeleteThread(*(SceUID *)0x09FFE800);

	InitPopsVram();

	/* Flash screen white */
	FlashScreen(0xFFFFFF);

	_memset(&rebootex_config, 0, sizeof(RebootexConfig));

	/* Get savedata path */
	const char *p = path;
	int len = 0;
	while(*p)
	{
		if(p[0] == '/' && p[1] == 'T' && p[2] == 'N')
		{
			break;
		}

		p++;
		len++;
	}

	_memcpy(rebootex_config.savedata_path, (void *)path, len);

	doExploit();

	while(1); //Infinite loop
}