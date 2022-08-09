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
#include "executable_patch.h"

#include "custom_png.h"

PSP_MODULE_INFO("PopsControl", 0x1007, 1, 0);

int (* RunReboot)(u32 *params);
int (* DecodeKL4E)(void *dest, u32 size_dest, void *src, u32 size_src);

int (* PrologueModule)(void *modmgr_param, SceModule2 *mod);

int OnModuleStart(SceModule2 *mod);

void *rebootex;
RebootexConfig rebootex_config;

#ifdef DEBUG

void logmsg(char *msg)
{
	int k1 = pspSdkSetK1(0);

	char string[128];
	sprintf(string, "%s/log.txt", rebootex_config.savedata_path);

	SceUID fd = sceIoOpen(string, PSP_O_WRONLY | PSP_O_CREAT, 0777);
	if(fd >= 0)
	{
		sceIoLseek(fd, 0, PSP_SEEK_END);
		sceIoWrite(fd, msg, strlen(msg));
		sceIoClose(fd);
	}

	pspSdkSetK1(k1);
}

#endif

int ReadFile(char *file, void *buf, int size)
{
	SceUID fd = sceIoOpen(file, PSP_O_RDONLY, 0);
	if(fd < 0) return fd;
	int read = sceIoRead(fd, buf, size);
	sceIoClose(fd);
	return read;
}

int WriteFile(char *file, void *buf, int size)
{
	SceUID fd = sceIoOpen(file, PSP_O_WRONLY | PSP_O_CREAT | PSP_O_TRUNC, 0777);
	if(fd < 0) return fd;
	int written = sceIoWrite(fd, buf, size);
	sceIoClose(fd);
	return written;
}

void ClearCaches()
{
	sceKernelIcacheInvalidateAll();
	sceKernelDcacheWritebackInvalidateAll();
}

u32 sctrlHENFindFunction(const char *szMod, const char *szLib, u32 nid)
{
	SceModule2 *mod = sceKernelFindModuleByName660(szMod);
	if(!mod) return 0;

	int i = 0;
	while(i < mod->ent_size)
	{
		SceLibraryEntryTable *entry = (SceLibraryEntryTable *)(mod->ent_top + i);

        if(entry->libname && strcmp(entry->libname, szLib) == 0)
		{
			u32 *table = entry->entrytable;
			int total = entry->stubcount + entry->vstubcount;

			int j;
			for(j = 0; j < total; j++)
			{
				if(table[j] == nid)
				{
					return table[j + total];
				}
			}
		}

		i += (entry->len * 4);
	}

	return 0;
}

u32 sctrlHENFindImport(const char *szMod, const char *szLib, u32 nid)
{
	SceModule2 *mod = sceKernelFindModuleByName660(szMod);
	if(!mod) return 0;

	int i = 0;
	while(i < mod->stub_size)
	{
		SceLibraryStubTable *stub = (SceLibraryStubTable *)(mod->stub_top + i);

		if(stub->libname && strcmp(stub->libname, szLib) == 0)
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

		i += (stub->len * 4);
	}

	return 0;
}

void sctrlHENPatchSyscall(u32 addr, void *newaddr)
{
	void *ptr;
	asm("cfc0 %0, $12\n" : "=r"(ptr));

	u32 *syscalls = (u32 *)(ptr + 0x10);

	int i;
	for(i = 0; i < 0x1000; i++)
	{
		if((syscalls[i] & 0x0FFFFFFF) == (addr & 0x0FFFFFFF))
		{
			syscalls[i] = (u32)newaddr;
		}
	}
}

int PrologueModulePatched(void *modmgr_param, SceModule2 *mod)
{
	int res = PrologueModule(modmgr_param, mod);

	if(res >= 0)
	{
		OnModuleStart(mod);
	}

	return res;
}

int MakeFileList()
{
	/* Open package */
	char path[64];
	sprintf(path, "%s/FLASH0.TN", rebootex_config.savedata_path);

	SceUID fd = sceIoOpen(path, PSP_O_RDONLY, 0);
	if(fd < 0) return fd;

	/* Get n_custom_files */
	int n_custom_files = 0;
	sceIoRead(fd, &n_custom_files, sizeof(int));

	/* Get n_files */
	int n_files = 0;
	FileList *orig_file_list = (FileList *)0x8B000000;
	while(orig_file_list[n_files].buffer != NULL) n_files++;

	/* Add custom files to file_list */
	FileList *file_list = (FileList *)0x8BA00000;
	memcpy((void *)&file_list[n_custom_files], (void *)orig_file_list, n_files * sizeof(FileList));
	file_list[n_files + n_custom_files].buffer = NULL; //end

	u32 buffer_pointer = (u32)&file_list[n_files + n_custom_files + 1];
	u32 path_pointer = 0x8BE00000;

	int res = 0;
	do
	{
		TNPackage package;
		res = sceIoRead(fd, &package, sizeof(TNPackage));

		if(res > 0)
		{
			if(package.magic == 0x4B504E54)
			{
				/* Add path */
				file_list->name = (void *)path_pointer;
				sceIoRead(fd, file_list->name, package.path_len);

				/* Add buffer */
				buffer_pointer = ALIGN(buffer_pointer, 0x40);
				file_list->buffer = (void *)buffer_pointer;
				sceIoRead(fd, (void *)0x8BC00000, package.size);
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

	sceIoClose(fd);

	return res;
}

int RunRebootPatched(u32 *params)
{
	MakeFileList();
	return RunReboot(params);
}

int DecodeKL4EPatched(void *dest, u32 size_dest, void *src, u32 size_src)
{
	memcpy((void *)0x88FC0000, rebootex, rebootex_config.rebootex_size);
	memcpy((void *)0x88FB0000, &rebootex_config, sizeof(RebootexConfig));
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

	ClearCaches();
}

void PatchInterruptMgr()
{
	SceModule2 *mod = sceKernelFindModuleByName660("sceInterruptManager");
	u32 text_addr = mod->text_addr;

	/* Allow execution of syscalls in kernel mode */
	_sw(0x408F7000, text_addr + 0xE98);
	_sw(0, text_addr + 0xE9C);
}

void PatchModuleMgr()
{
	SceModule2 *mod = sceKernelFindModuleByName660("sceModuleManager");
	u32 text_addr = mod->text_addr;

	int i;
	for(i = 0; i < mod->text_size; i += 4)
	{
		u32 addr = text_addr + i;

		if(_lw(addr) == 0xA4A60024)
		{
			/* Patch to allow a full coverage of loaded modules */
			PrologueModule = (void *)K_EXTRACT_CALL(addr - 4);
			MAKE_CALL(addr - 4, PrologueModulePatched);
			continue;
		}

		if(_lw(addr) == 0x27BDFFE0 && _lw(addr + 4) == 0xAFB10014)
		{
			HIJACK_FUNCTION(addr, PartitionCheckPatched, PartitionCheck);
			continue;
		}
	}
}

void PatchLoadCore()
{
	SceModule2 *mod = sceKernelFindModuleByName660("sceLoaderCore");
	u32 text_addr = mod->text_addr;

	HIJACK_FUNCTION(K_EXTRACT_IMPORT(&sceKernelCheckExecFile660), sceKernelCheckExecFilePatched, _sceKernelCheckExecFile);
	HIJACK_FUNCTION(K_EXTRACT_IMPORT(&sceKernelProbeExecutableObject660), sceKernelProbeExecutableObjectPatched, _sceKernelProbeExecutableObject);

	int i;
	for(i = 0; i < mod->text_size; i += 4)
	{
		u32 addr = text_addr + i;

		/* Allow custom modules */
		if(_lw(addr) == 0x1440FF55)
		{
			PspUncompress = (void *)K_EXTRACT_CALL(addr - 8);
			MAKE_CALL(addr - 8, PspUncompressPatched);
			continue;
		}

		/* Patch relocation check in switch statement (7 -> 0) */
		if(_lw(addr) == 0x00A22021)
		{
			u32 high = (((u32)_lh(addr - 0xC)) << 16);
			u32 low = ((u32)_lh(addr - 0x4));

			if(low & 0x8000) high -= 0x10000;

			u32 *RelocationTable = (u32 *)(high | low);

			RelocationTable[7] = RelocationTable[0];

			continue;
		}

		/* Allow kernel modules to have syscall imports */
		if(_lw(addr) == 0x30894000)
		{
			_sw(0x3C090000, addr);
			continue;
		}

		/* Allow lower devkit version */
		if(_lw(addr) == 0x14A0FFCB)
		{
			_sh(0x1000, addr + 2);
			continue;
		}

		/* Allow higher devkit version */
		if(_lw(addr) == 0x14C0FFDF)
		{
			_sw(0, addr);
			continue;
		}

		/* Restore original call */
		if(_lw(addr) == 0xAE2D0048)
		{
			MAKE_CALL(addr + 8, FindProc("sceMemlmd", "memlmd", 0xEF73E85B));
			continue;
		}
	}
}

/* PSX CODE */

int is_custom_psx = 0;
int use_custom_png = 1;

int (* SetKeys)(char *filename, void *keys, void *keys2);
int (* scePopsSetKeys)(int size, void *keys, void *keys2);

int (* scePopsManExitVSHKernel)(u32 error);

SceUID sceIoOpenPatched(const char *file, int flags, SceMode mode)
{
	return sceIoOpen(file, (flags & 0x40000000) ? (flags & ~0x40000000) : flags, mode);
}

int sceIoIoctlPatched(SceUID fd, unsigned int cmd, void *indata, int inlen, void *outdata, int outlen)
{
	int res = sceIoIoctl(fd, cmd, indata, inlen, outdata, outlen);

	if(res < 0)
	{
		if(cmd == 0x04100001)
		{
			return 0;
		}
		else if(cmd == 0x04100002)
		{
			u32 offset = *(u32 *)indata;		
			sceIoLseek(fd, offset, PSP_SEEK_SET);
			return 0;
		}
	}
	
	return res;
}

int sceIoReadPatched(SceUID fd, void *data, SceSize size)
{
	int k1 = pspSdkSetK1(0);
	int res = sceIoRead(fd, data, size);

	if(res == size)
	{
		if(size == size_custom_png)
		{
			if(use_custom_png)
			{
				u32 magic = 0x474E5089;
				if(memcmp(data, &magic, sizeof(u32)) == 0)
				{
					memcpy(data, custom_png, size_custom_png);
					res = size_custom_png;
					goto RETURN;
				}
			}
		}
		else if(size == 4)
		{
			u32 magic = 0x464C457F;
			if(memcmp(data, &magic, sizeof(u32)) == 0)
			{
				magic = 0x5053507E;
				memcpy(data, &magic, sizeof(u32));
				goto RETURN;
			}
		}

		/* Castlevania sound patch */
		if(size >= 0x420)
		{
			if(((u8 *)data)[0x41B] == 0x27 && ((u8 *)data)[0x41C] == 0x19)
			{
				if(((u8 *)data)[0x41D] == 0x22 && ((u8 *)data)[0x41E] == 0x41)
				{
					if(((u8 *)data)[0x41A] == ((u8 *)data)[0x41F])
					{
						((u8 *)data)[0x41B] = 0x55;
					}
				}
			}
		}
	}

RETURN:
	pspSdkSetK1(k1);
	return res;
}

int SetKeysPatched(char *filename, u8 *keys, u8 *keys2)
{
	char path[64];
	strcpy(path, filename);

	char *p = strrchr(path, '/');
	if(!p) return 0xCA000000;

	strcpy(p + 1, "KEYS.BIN");

	if(ReadFile(path, keys, 0x10) != 0x10)
	{
		SceUID fd = sceIoOpen(filename, PSP_O_RDONLY, 0777);
		if(fd >= 0)
		{
			u32 header[0x28/4];
			sceIoRead(fd, header, 0x28);
			sceIoLseek(fd, header[0x20/4], PSP_SEEK_SET);
			sceIoRead(fd, header, 4);
			sceIoClose(fd);

			if(header[0] == 0x464C457F)
			{
				memset(keys, 'X', 0x10);
				goto SET_KEYS;
			}
		}

		int res = SetKeys(filename, keys, keys2);
		if(res >= 0) WriteFile(path, keys, 0x10);
		return res;
	}

SET_KEYS:
	scePopsSetKeys(0x10, keys, keys);
	return 0;
}

int scePopsManExitVSHKernelPatched(u32 destSize, u8 *src, u8 *dest)
{
	int k1 = pspSdkSetK1(0);

	if(destSize & 0x80000000)
	{
		scePopsManExitVSHKernel(destSize);
		pspSdkSetK1(k1);
		return 0;
	}

	int size = sceKernelDeflateDecompress(dest, destSize, src, 0);
	pspSdkSetK1(k1);

	return (size ^ 0x9300 ? size : 0x92FF);
}

void PatchMediaSync(u32 text_addr)
{
	/* Dummy all checks...PBP header, SFO header */
	MAKE_DUMMY_FUNCTION(text_addr + 0x000006A8, 0);

	/* Avoid SCE_MEDIASYNC_ERROR_INVALID_MEDIA */
	_sh(0x5000, text_addr + 0x00000328 + 2);
	_sh(0x1000, text_addr + 0x00000D2C + 2);

	ClearCaches();
}

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

/*
	POPS resolution: 640x480
	POPS framesize: 640x480x2
*/

u32 framebuffer = 0;

static u16 convert_8888_to_5551(u32 color)
{
	int r, g, b, a;

	a = (color >> 24) ? 0x8000 : 0;
	b = (color >> 19) & 0x1F;
	g = (color >> 11) & 0x1F;
	r = (color >> 3) & 0x1F;

	return a | r | (g << 5) | (b << 10);
}

u32 GetPopsVramAddr(int x, int y)
{
	return 0x490C0000 + x * 2 + y * 640 * 4;
}

u32 GetPspVramAddr(int x, int y)
{
	return framebuffer + x * 4 + y * 512 * 4;
}

void RelocateVram()
{
	if(framebuffer)
	{
		int y;
		for(y = 0; y < 272; y++)
		{
			int x;
			for(x = 0; x < 480; x++)
			{
				u32 color = *(u32 *)GetPspVramAddr(x, y);
				*(u16 *)GetPopsVramAddr(x, y) = convert_8888_to_5551(color);
			}
		}
	}
}

int (* _sceDisplaySetFrameBufferInternal)(int pri, void *topaddr, int bufferwidth, int pixelformat, int sync);

int sceDisplaySetFrameBufferInternalPatched(int pri, void *topaddr, int bufferwidth, int pixelformat, int sync)
{
	framebuffer = (u32)topaddr;
	RelocateVram();
	return _sceDisplaySetFrameBufferInternal(pri, topaddr, bufferwidth, pixelformat, sync); 
}

int OnModuleStart(SceModule2 *mod)
{
	char *modname = mod->modname;
	u32 text_addr = mod->text_addr;

	if(strcmp(modname, "sceLoadExec") == 0)
	{
		PatchLoadExec(text_addr, mod->text_size);
	}
	else if(strcmp(modname, "sceDisplay_Service") == 0)
	{
		HIJACK_FUNCTION(FindProc("sceDisplay_Service", "sceDisplay_driver", 0x3E17FE8D), sceDisplaySetFrameBufferInternalPatched, _sceDisplaySetFrameBufferInternal);
		ClearCaches();
	}
	else if(strcmp(modname, "sceMediaSync") == 0)
	{
		PatchMediaSync(text_addr);
	}
	else if(strcmp(modname, "scePops_Manager") == 0)
	{
		SceUID fd = sceIoOpen(sceKernelInitFileName(), PSP_O_RDONLY, 0);
		if(fd >= 0)
		{
			PBPHeader header;
			sceIoRead(fd, &header, sizeof(PBPHeader));

			u32 pgd_offset = header.psar_offset;
			u32 icon0_offset = header.icon0_offset;

			u8 buffer[8];
			sceIoLseek(fd, header.psar_offset, PSP_SEEK_SET);
			sceIoRead(fd, buffer, 7);

			if(memcmp(buffer, "PSTITLE", 7) == 0) //official psx game
			{
				pgd_offset += 0x200;
			}
			else
			{
				pgd_offset += 0x400;
			}

			u32 pgd_header;
			sceIoLseek(fd, pgd_offset, PSP_SEEK_SET);
			sceIoRead(fd, &pgd_header, sizeof(u32));

			/* Is not PGD header */
			if(pgd_header != 0x44475000)
			{
				is_custom_psx = 1;

				u32 icon_header[6];
				sceIoLseek(fd, icon0_offset, PSP_SEEK_SET);
				sceIoRead(fd, icon_header, sizeof(icon_header));

				/* Check 80x80 PNG */
				if(icon_header[0] == 0x474E5089 &&
				   icon_header[1] == 0x0A1A0A0D &&
				   icon_header[3] == 0x52444849 &&
				   icon_header[4] == 0x50000000 &&
				   icon_header[5] == 0x50000000)
				{
					use_custom_png = 0;
				}

				scePopsManExitVSHKernel = (void *)FindProc("scePops_Manager", "scePopsMan", 0x0090B2C8);
				sctrlHENPatchSyscall((u32)scePopsManExitVSHKernel, scePopsManExitVSHKernelPatched);

				/* Patch IO */
				MAKE_JUMP(sctrlHENFindImport(modname, "IoFileMgrForKernel", 0x109F50BC), sceIoOpenPatched);
				MAKE_JUMP(sctrlHENFindImport(modname, "IoFileMgrForKernel", 0x63632449), sceIoIoctlPatched);
				MAKE_JUMP(sctrlHENFindImport(modname, "IoFileMgrForKernel", 0x6A638D83), sceIoReadPatched);

				/* Dummy amctrl decryption functions */
				MAKE_DUMMY_FUNCTION(text_addr + 0x00000E84, 0);
				MAKE_DUMMY_FUNCTION(FindProc(modname, "sceMeAudio", 0xF6637A72), 1);
				_sw(0, text_addr + 0x0000053C);

				/* Allow lower compiled sdk versions */
				_sw(0, text_addr + 0x000010D0);
			}

			sceIoClose(fd);

			scePopsSetKeys = (void *)text_addr + 0x00000124;

			HIJACK_FUNCTION(text_addr + 0x000014FC, SetKeysPatched, SetKeys);

			ClearCaches();
		}
	}
	else if(strcmp(modname, "pops") == 0)
	{
		if(is_custom_psx)
		{
			/* Use our decompression function */
			_sw(_lw(text_addr + 0x0000C6A4), text_addr + 0x0000C9A4);

			/* Patch PNG size */
			if(use_custom_png)
			{
				_sw(0x24050000 | (size_custom_png & 0xFFFF), text_addr + 0x00017C38);
			}
		}

		ClearCaches();
	}

	return 0;
}

int module_start()
{
	/* Backup rebootex config */
	memcpy(&rebootex_config, (void *)0x88FB0000, sizeof(RebootexConfig));

	/* Backup rebootex binary */
	SceUID block_id = sceKernelAllocPartitionMemory(PSP_MEMORY_PARTITION_KERNEL, "", PSP_SMEM_Low, rebootex_config.rebootex_size, NULL);
	if(block_id >= 0)
	{
	    rebootex = sceKernelGetBlockHeadAddr(block_id);
		memcpy(rebootex, (void *)0x88FC0000, rebootex_config.rebootex_size);
	}

	/* Protect memory */
	sceKernelAllocPartitionMemory(6, "", PSP_SMEM_Addr, 0x1B0, (void *)0x09FE0000);
	sceKernelAllocPartitionMemory(6, "", PSP_SMEM_Addr, 0x3C0000, (void *)0x090C0000);
	memset((void *)0x49FE0000, 0, 0x1B0);
	memset((void *)0x490C0000, 0, 0x3C0000);

	InitPopsVram();

	/* Patch already loaded modules */
	PatchLoadCore();
	PatchInterruptMgr();
	PatchModuleMgr();
	ClearCaches();

	return 0;
}