	.set noreorder

#include "pspimport.s"

	IMPORT_START "LoadCoreForKernel",0x00010011
	IMPORT_FUNC  "LoadCoreForKernel",0xF6B1BF0F,sceKernelFindModuleByName660
	IMPORT_FUNC  "LoadCoreForKernel",0xD3353EC4,sceKernelCheckExecFile660
	IMPORT_FUNC  "LoadCoreForKernel",0x41D10899,sceKernelProbeExecutableObject660