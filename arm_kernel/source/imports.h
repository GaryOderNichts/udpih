#pragma once

#include <stdlib.h>
#include <stdint.h>
#include <stdarg.h>
#include <string.h>

typedef struct {
    void* ptr;
    uint32_t len;
    uint32_t paddr;
} IOSVec_t;

typedef struct {
    uint32_t paddr;
    uint32_t vaddr;
    uint32_t size;
    uint32_t domain;
    // 0 = undefined, 1 = kernel only, 2 = read only, 3 = read write
    uint32_t type;
    uint32_t cached;
} ios_map_shared_info_t;

int disable_interrupts(void);
int enable_interrupts(int);

void invalidate_icache(void);
void invalidate_dcache(void* ptr, uint32_t size);
void flush_dcache(void* ptr, uint32_t size);
int setClientCapabilities(int processId, int featureId, uint64_t featureMask);
int _iosMapSharedUserExecution(ios_map_shared_info_t* info);

void* IOS_HeapAlloc(uint32_t heap, uint32_t size);
void* IOS_HeapAllocAligned(uint32_t heap, uint32_t size, uint32_t alignment);
void IOS_HeapFree(uint32_t heap, void* ptr);

int IOS_Open(const char* device, int mode);
int IOS_Close(int fd);
int IOS_Ioctl(int fd, uint32_t request, void* input_buffer, uint32_t input_buffer_len, void* output_buffer, uint32_t output_buffer_len);
int IOS_Ioctlv(int fd, uint32_t request, uint32_t vector_count_in, uint32_t vector_count_out, IOSVec_t* vector);

void* IOS_VirtToPhys(void* ptr);

void IOS_Shutdown(int reset);
void IOS_Reset(void);

static inline uint32_t disable_mmu(void)
{
    uint32_t control_register = 0;
    asm volatile("MRC p15, 0, %0, c1, c0, 0" : "=r" (control_register));
    asm volatile("MCR p15, 0, %0, c1, c0, 0" : : "r" (control_register & 0xFFFFEFFA));
    return control_register;
}

static inline void restore_mmu(uint32_t control_register)
{
    asm volatile("MCR p15, 0, %0, c1, c0, 0" : : "r" (control_register));
}
