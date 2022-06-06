PROVIDE(memcpy = 0x08131d04);
PROVIDE(memset = 0x08131da0);
PROVIDE(strncpy = 0x081329b8);
PROVIDE(vsnprintf = 0x0813293c);
PROVIDE(snprintf = 0x08132988);

PROVIDE(disable_interrupts = 0x0812e778);
PROVIDE(enable_interrupts = 0x0812e78c);

PROVIDE(invalidate_icache = 0x0812dcf0);
PROVIDE(invalidate_dcache = 0x08120164);
PROVIDE(flush_dcache = 0x081200f0);
PROVIDE(setClientCapabilities = 0x081260a8);
PROVIDE(_iosMapSharedUserExecution = 0x08124f88);

PROVIDE(IOS_HeapAlloc = 0x081234e4);
PROVIDE(IOS_HeapAllocAligned = 0x08123464);
PROVIDE(IOS_HeapFree = 0x08123830);

PROVIDE(IOS_Open = 0x0812940c);
PROVIDE(IOS_Close = 0x08129368);
PROVIDE(IOS_Ioctl = 0x081290e0);
PROVIDE(IOS_Ioctlv = 0x0812903c);

PROVIDE(IOS_VirtToPhys = 0x08124484);
PROVIDE(IOS_Shutdown = 0xffffdc48);
PROVIDE(IOS_Reset = 0x08129760);

PROVIDE(disableMMU = 0x0812ea6c);
PROVIDE(enableMMU = 0x0812ea5c);
