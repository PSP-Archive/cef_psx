#ifndef __COMMON_H__
#define __COMMON_H__

#include <pspsdk.h>
#include <pspkernel.h>

#include <psperror.h>

#include <systemctrl.h>
#include <systemctrl_se.h>

#include <pspsysevent.h>
#include <psputilsforkernel.h>
#include <pspsysmem_kernel.h>
#include <psploadexec_kernel.h>
#include <pspthreadman_kernel.h>

#include <pspumd.h>
#include <psprtc.h>
#include <pspreg.h>
#include <pspctrl.h>
#include <psppower.h>
#include <pspdisplay.h>
#include <psputility.h>

#include <malloc.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

#include <registry_info.h>

#define TN_V_VERSION_MAJOR 8
#define TN_V_VERSION_MINOR 0

#define PBP_MAGIC 0x50425000
#define ELF_MAGIC 0x464C457F

#define FW_TO_FIRMWARE(f) ((((f >> 8) & 0xF) << 24) | (((f >> 4) & 0xF) << 16) | ((f & 0xF) << 8) | 0x10)
#define FIRMWARE_TO_FW(f) ((((f >> 24) & 0xF) << 8) | (((f >> 16) & 0xF) << 4) | ((f >> 8) & 0xF))

#define MAKE_JUMP(a, f) _sw(0x08000000 | (((u32)(f) & 0x0FFFFFFC) >> 2), a);
#define MAKE_CALL(a, f) _sw(0x0C000000 | (((u32)(f) >> 2) & 0x03FFFFFF), a);

#define MAKE_SYSCALL_CALL(a, n) _sw(0x0000000C | (n << 6), a);

#define MAKE_SYSCALL_JUMP(a, n) \
{ \
	u32 func = a; \
	_sw(0x03E00008, func); \
	_sw(0x0000000C | (n << 6), func); \
}

#define REDIRECT_FUNCTION(a, f) \
{ \
	u32 func = a; \
	_sw(0x08000000 | (((u32)(f) >> 2) & 0x03FFFFFF), func); \
	_sw(0, func + 4); \
}

#define MAKE_DUMMY_FUNCTION(a, r) \
{ \
	u32 func = a; \
	if(r == 0) \
	{ \
		_sw(0x03E00008, func); \
		_sw(0x00001021, func + 4); \
	} \
	else \
	{ \
		_sw(0x03E00008, func); \
		_sw(0x24020000 | r, func + 4); \
	} \
}

//by Davee
#define HIJACK_FUNCTION(a, f, ptr) \
{ \
	u32 func = a; \
	static u32 patch_buffer[3]; \
	_sw(_lw(func), (u32)patch_buffer); \
	_sw(_lw(func + 4), (u32)patch_buffer + 8);\
	MAKE_JUMP((u32)patch_buffer + 4, func + 8); \
	_sw(0x08000000 | (((u32)(f) >> 2) & 0x03FFFFFF), func); \
	_sw(0, func + 4); \
	ptr = (void *)patch_buffer; \
}

//by Bubbletune
#define U_EXTRACT_IMPORT(x) ((((u32)_lw((u32)x)) & ~0x08000000) << 2)
#define K_EXTRACT_IMPORT(x) (((((u32)_lw((u32)x)) & ~0x08000000) << 2) | 0x80000000)
#define U_EXTRACT_CALL(x) ((((u32)_lw((u32)x)) & ~0x0C000000) << 2)
#define K_EXTRACT_CALL(x) (((((u32)_lw((u32)x)) & ~0x0C000000) << 2) | 0x80000000)

#define ALIGN(x, align) (((x)+((align)-1))&~((align)-1))

enum FirmwareVersion
{
	FW_UNK = -1,
	FW_1XX,
	FW_20X,
	//FW_206
	FW_21X,
	FW_26X,
	FW_300,
	FW_301_AND_LATER,
};

typedef struct
{
	u32 magic;
	u32 version;
	u32 param_offset;
	u32 icon0_offset;
	u32 icon1_offset;
	u32 pic0_offset;
	u32 pic1_offset;
	u32 snd0_offset;
	u32 elf_offset;
	u32 psar_offset;
} PBPHeader;

typedef struct  __attribute__((packed))
{
	u32 signature;
	u32 version;
	u32 fields_table_offs;
	u32 values_table_offs;
	int nitems;
} SFOHeader;

typedef struct __attribute__((packed))
{
	u16 field_offs;
	u8  unk;
	u8  type; // 0x2 -> string, 0x4 -> number
	u32 unk2;
	u32 unk3;
	u16 val_offs;
	u16 unk4;
} SFODir;

typedef struct
{
	char *name;
	void *buffer;
	u32 size;
} BootFileInfo;

typedef struct
{
	char *name;
	void *buffer;
	u32 size;
} FileList;

typedef struct
{
	u32 magic;
	u32 size;
	u32 path_len;
} TNPackage;

typedef struct
{
	char savedata_path[64];
	int rebootex_size;
} RebootexConfig;

void sctrlGetRebootexConfig(RebootexConfig *config);

#endif