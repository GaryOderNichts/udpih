#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "imports.h"
#include "fsa.h"

static void* allocIobuf()
{
    void* ptr = IOS_HeapAlloc(0xcaff, 0x828);

    memset(ptr, 0x00, 0x828);

    return ptr;
}

static void freeIobuf(void* ptr)
{
    IOS_HeapFree(0xcaff, ptr);
}

int FSA_Mount(int fd, const char* device_path, char* volume_path, uint32_t flags, char* arg_string, int arg_string_len)
{
    uint8_t* iobuf = allocIobuf();
    uint8_t* inbuf8 = iobuf;
    uint8_t* outbuf8 = &iobuf[0x520];
    IOSVec_t* iovec = (IOSVec_t*)&iobuf[0x7C0];
    uint32_t* inbuf = (uint32_t*)inbuf8;
    uint32_t* outbuf = (uint32_t*)outbuf8;

    strncpy((char*)&inbuf8[0x04], device_path, 0x27F);
    strncpy((char*)&inbuf8[0x284], volume_path, 0x27F);
    inbuf[0x504 / 4] = (uint32_t)flags;
    inbuf[0x508 / 4] = (uint32_t)arg_string_len;

    iovec[0].ptr = inbuf;
    iovec[0].len = 0x520;
    iovec[1].ptr = arg_string;
    iovec[1].len = arg_string_len;
    iovec[2].ptr = outbuf;
    iovec[2].len = 0x293;

    int ret = IOS_Ioctlv(fd, 0x01, 2, 1, iovec);

    freeIobuf(iobuf);
    return ret;
}

int FSA_Unmount(int fd, const char* path, uint32_t flags)
{
    uint8_t* iobuf = allocIobuf();
    uint32_t* inbuf = (uint32_t*)iobuf;
    uint32_t* outbuf = (uint32_t*)&iobuf[0x520];

    strncpy((char*)&inbuf[0x01], path, 0x27F);
    inbuf[0x284 / 4] = flags;

    int ret = IOS_Ioctl(fd, 0x02, inbuf, 0x520, outbuf, 0x293);

    freeIobuf(iobuf);
    return ret;
}

int FSA_FlushVolume(int fd, const char* volume_path)
{
    uint8_t* iobuf = allocIobuf();
    uint32_t* inbuf = (uint32_t*)iobuf;
    uint32_t* outbuf = (uint32_t*)&iobuf[0x520];

    strncpy((char*)&inbuf[0x01], volume_path, 0x27F);

    int ret = IOS_Ioctl(fd, 0x1B, inbuf, 0x520, outbuf, 0x293);

    freeIobuf(iobuf);
    return ret;
}

int FSA_MakeDir(int fd, const char* path, uint32_t flags)
{
    uint8_t* iobuf = allocIobuf();
    uint32_t* inbuf = (uint32_t*)iobuf;
    uint32_t* outbuf = (uint32_t*)&iobuf[0x520];

    strncpy((char*)&inbuf[0x01], path, 0x27F);
    inbuf[0x284 / 4] = flags;

    int ret = IOS_Ioctl(fd, 0x07, inbuf, 0x520, outbuf, 0x293);

    freeIobuf(iobuf);
    return ret;
}

int FSA_OpenDir(int fd, const char* path, int* outHandle)
{
    uint8_t* iobuf = allocIobuf();
    uint32_t* inbuf = (uint32_t*)iobuf;
    uint32_t* outbuf = (uint32_t*)&iobuf[0x520];

    strncpy((char*)&inbuf[0x01], path, 0x27F);

    int ret = IOS_Ioctl(fd, 0x0A, inbuf, 0x520, outbuf, 0x293);

    if(outHandle) *outHandle = outbuf[1];

    freeIobuf(iobuf);
    return ret;
}

int FSA_ReadDir(int fd, int handle, FSDirectoryEntry* out_data)
{
    uint8_t* iobuf = allocIobuf();
    uint32_t* inbuf = (uint32_t*)iobuf;
    uint32_t* outbuf = (uint32_t*)&iobuf[0x520];

    inbuf[1] = handle;

    int ret = IOS_Ioctl(fd, 0x0B, inbuf, 0x520, outbuf, 0x293);

    if(out_data) memcpy(out_data, &outbuf[1], sizeof(FSDirectoryEntry));

    freeIobuf(iobuf);
    return ret;
}

int FSA_RewindDir(int fd, int handle)
{
    uint8_t* iobuf = allocIobuf();
    uint32_t* inbuf = (uint32_t*)iobuf;
    uint32_t* outbuf = (uint32_t*)&iobuf[0x520];

    inbuf[1] = handle;

    int ret = IOS_Ioctl(fd, 0x0C, inbuf, 0x520, outbuf, 0x293);

    freeIobuf(iobuf);
    return ret;
}

int FSA_CloseDir(int fd, int handle)
{
    uint8_t* iobuf = allocIobuf();
    uint32_t* inbuf = (uint32_t*)iobuf;
    uint32_t* outbuf = (uint32_t*)&iobuf[0x520];

    inbuf[1] = handle;

    int ret = IOS_Ioctl(fd, 0x0D, inbuf, 0x520, outbuf, 0x293);

    freeIobuf(iobuf);
    return ret;
}

int FSA_ChangeDir(int fd, const char* path)
{
    uint8_t* iobuf = allocIobuf();
    uint32_t* inbuf = (uint32_t*)iobuf;
    uint32_t* outbuf = (uint32_t*)&iobuf[0x520];

    strncpy((char*)&inbuf[0x01], path, 0x27F);

    int ret = IOS_Ioctl(fd, 0x05, inbuf, 0x520, outbuf, 0x293);

    freeIobuf(iobuf);
    return ret;
}

int FSA_OpenFile(int fd, const char* path, const char* mode, int* outHandle)
{
    uint8_t* iobuf = allocIobuf();
    uint32_t* inbuf = (uint32_t*)iobuf;
    uint32_t* outbuf = (uint32_t*)&iobuf[0x520];

    strncpy((char*)&inbuf[0x01], path, 0x27F);
    strncpy((char*)&inbuf[0xA1], mode, 0x10);

    int ret = IOS_Ioctl(fd, 0x0E, inbuf, 0x520, outbuf, 0x293);

    if(outHandle) *outHandle = outbuf[1];

    freeIobuf(iobuf);
    return ret;
}

int _FSA_ReadWriteFile(int fd, void* data, uint32_t size, uint32_t cnt, int fileHandle, uint32_t flags, int read)
{
    uint8_t* iobuf = allocIobuf();
    uint8_t* inbuf8 = iobuf;
    uint8_t* outbuf8 = &iobuf[0x520];
    IOSVec_t* iovec = (IOSVec_t*)&iobuf[0x7C0];
    uint32_t* inbuf = (uint32_t*)inbuf8;
    uint32_t* outbuf = (uint32_t*)outbuf8;

    inbuf[0x08 / 4] = size;
    inbuf[0x0C / 4] = cnt;
    inbuf[0x14 / 4] = fileHandle;
    inbuf[0x18 / 4] = flags;

    iovec[0].ptr = inbuf;
    iovec[0].len = 0x520;

    iovec[1].ptr = data;
    iovec[1].len = size * cnt;

    iovec[2].ptr = outbuf;
    iovec[2].len = 0x293;

    int ret;
    if(read) ret = IOS_Ioctlv(fd, 0x0F, 1, 2, iovec);
    else ret = IOS_Ioctlv(fd, 0x10, 2, 1, iovec);

    freeIobuf(iobuf);
    return ret;
}

int FSA_ReadFile(int fd, void* data, uint32_t size, uint32_t cnt, int fileHandle, uint32_t flags)
{
    return _FSA_ReadWriteFile(fd, data, size, cnt, fileHandle, flags, 1);
}

int FSA_WriteFile(int fd, void* data, uint32_t size, uint32_t cnt, int fileHandle, uint32_t flags)
{
    return _FSA_ReadWriteFile(fd, data, size, cnt, fileHandle, flags, 0);
}

int FSA_StatFile(int fd, int handle, FSStat* out_data)
{
    uint8_t* iobuf = allocIobuf();
    uint32_t* inbuf = (uint32_t*)iobuf;
    uint32_t* outbuf = (uint32_t*)&iobuf[0x520];

    inbuf[1] = handle;

    int ret = IOS_Ioctl(fd, 0x14, inbuf, 0x520, outbuf, 0x293);

    if(out_data) memcpy(out_data, &outbuf[1], sizeof(FSStat));

    freeIobuf(iobuf);
    return ret;
}

int FSA_CloseFile(int fd, int fileHandle)
{
    uint8_t* iobuf = allocIobuf();
    uint32_t* inbuf = (uint32_t*)iobuf;
    uint32_t* outbuf = (uint32_t*)&iobuf[0x520];

    inbuf[1] = fileHandle;

    int ret = IOS_Ioctl(fd, 0x15, inbuf, 0x520, outbuf, 0x293);

    freeIobuf(iobuf);
    return ret;
}

int FSA_SetPosFile(int fd, int fileHandle, uint32_t position)
{
    uint8_t* iobuf = allocIobuf();
    uint32_t* inbuf = (uint32_t*)iobuf;
    uint32_t* outbuf = (uint32_t*)&iobuf[0x520];

    inbuf[1] = fileHandle;
    inbuf[2] = position;

    int ret = IOS_Ioctl(fd, 0x12, inbuf, 0x520, outbuf, 0x293);

    freeIobuf(iobuf);
    return ret;
}

int FSA_GetStat(int fd, const char *path, FSStat* out_data)
{
    uint8_t* iobuf = allocIobuf();
    uint32_t* inbuf = (uint32_t*)iobuf;
    uint32_t* outbuf = (uint32_t*)&iobuf[0x520];

    strncpy((char*)&inbuf[0x01], path, 0x27F);
    inbuf[0x284/4] = 5;

    int ret = IOS_Ioctl(fd, 0x18, inbuf, 0x520, outbuf, 0x293);

    if(out_data) memcpy(out_data, &outbuf[1], sizeof(FSStat));

    freeIobuf(iobuf);
    return ret;
}

int FSA_Remove(int fd, const char *path)
{
    uint8_t* iobuf = allocIobuf();
    uint32_t* inbuf = (uint32_t*)iobuf;
    uint32_t* outbuf = (uint32_t*)&iobuf[0x520];

    strncpy((char*)&inbuf[0x01], path, 0x27F);

    int ret = IOS_Ioctl(fd, 0x08, inbuf, 0x520, outbuf, 0x293);

    freeIobuf(iobuf);
    return ret;
}

int FSA_ChangeMode(int fd, const char *path, int mode)
{
    uint8_t* iobuf = allocIobuf();
    uint32_t* inbuf = (uint32_t*)iobuf;
    uint32_t* outbuf = (uint32_t*)&iobuf[0x520];

    strncpy((char*)&inbuf[0x01], path, 0x27F);
    inbuf[0x284/4] = mode;
    inbuf[0x288/4] = 0x777; // mask

    int ret = IOS_Ioctl(fd, 0x20, inbuf, 0x520, outbuf, 0x293);

    freeIobuf(iobuf);
    return ret;
}

// type 4 :
// 		0x08 : device size in sectors (uint64_t)
// 		0x10 : device sector size (uint32_t)
int FSA_GetDeviceInfo(int fd, const char* device_path, int type, uint32_t* out_data)
{
    uint8_t* iobuf = allocIobuf();
    uint32_t* inbuf = (uint32_t*)iobuf;
    uint32_t* outbuf = (uint32_t*)&iobuf[0x520];

    strncpy((char*)&inbuf[0x01], device_path, 0x27F);
    inbuf[0x284 / 4] = type;

    int ret = IOS_Ioctl(fd, 0x18, inbuf, 0x520, outbuf, 0x293);

    int size = 0;

    switch(type)
    {
        case 0: case 1: case 7:
            size = 0x8;
            break;
        case 2:
            size = 0x4;
            break;
        case 3:
            size = 0x1E;
            break;
        case 4:
            size = 0x28;
            break;
        case 5:
            size = 0x64;
            break;
        case 6: case 8:
            size = 0x14;
            break;
    }

    memcpy(out_data, &outbuf[1], size);

    freeIobuf(iobuf);
    return ret;
}

int FSA_RawOpen(int fd, const char* device_path, int* outHandle)
{
    uint8_t* iobuf = allocIobuf();
    uint32_t* inbuf = (uint32_t*)iobuf;
    uint32_t* outbuf = (uint32_t*)&iobuf[0x520];

    strncpy((char*)&inbuf[0x01], device_path, 0x27F);

    int ret = IOS_Ioctl(fd, 0x6A, inbuf, 0x520, outbuf, 0x293);

    if(outHandle) *outHandle = outbuf[1];

    freeIobuf(iobuf);
    return ret;
}

int FSA_RawClose(int fd, int device_handle)
{
    uint8_t* iobuf = allocIobuf();
    uint32_t* inbuf = (uint32_t*)iobuf;
    uint32_t* outbuf = (uint32_t*)&iobuf[0x520];

    inbuf[1] = device_handle;

    int ret = IOS_Ioctl(fd, 0x6D, inbuf, 0x520, outbuf, 0x293);

    freeIobuf(iobuf);
    return ret;
}

// offset in blocks of 0x1000 bytes
int FSA_RawRead(int fd, void* data, uint32_t size_bytes, uint32_t cnt, uint64_t blocks_offset, int device_handle)
{
    uint8_t* iobuf = allocIobuf();
    uint8_t* inbuf8 = iobuf;
    uint8_t* outbuf8 = &iobuf[0x520];
    IOSVec_t* iovec = (IOSVec_t*)&iobuf[0x7C0];
    uint32_t* inbuf = (uint32_t*)inbuf8;
    uint32_t* outbuf = (uint32_t*)outbuf8;

    // note : offset_bytes = blocks_offset * size_bytes
    inbuf[0x08 / 4] = (blocks_offset >> 32);
    inbuf[0x0C / 4] = (blocks_offset & 0xFFFFFFFF);
    inbuf[0x10 / 4] = cnt;
    inbuf[0x14 / 4] = size_bytes;
    inbuf[0x18 / 4] = device_handle;

    iovec[0].ptr = inbuf;
    iovec[0].len = 0x520;

    iovec[1].ptr = data;
    iovec[1].len = size_bytes * cnt;

    iovec[2].ptr = outbuf;
    iovec[2].len = 0x293;

    int ret = IOS_Ioctlv(fd, 0x6B, 1, 2, iovec);

    freeIobuf(iobuf);
    return ret;
}

int FSA_RawWrite(int fd, void* data, uint32_t size_bytes, uint32_t cnt, uint64_t blocks_offset, int device_handle)
{
    uint8_t* iobuf = allocIobuf();
    uint8_t* inbuf8 = iobuf;
    uint8_t* outbuf8 = &iobuf[0x520];
    IOSVec_t* iovec = (IOSVec_t*)&iobuf[0x7C0];
    uint32_t* inbuf = (uint32_t*)inbuf8;
    uint32_t* outbuf = (uint32_t*)outbuf8;

    inbuf[0x08 / 4] = (blocks_offset >> 32);
    inbuf[0x0C / 4] = (blocks_offset & 0xFFFFFFFF);
    inbuf[0x10 / 4] = cnt;
    inbuf[0x14 / 4] = size_bytes;
    inbuf[0x18 / 4] = device_handle;

    iovec[0].ptr = inbuf;
    iovec[0].len = 0x520;

    iovec[1].ptr = data;
    iovec[1].len = size_bytes * cnt;

    iovec[2].ptr = outbuf;
    iovec[2].len = 0x293;

    int ret = IOS_Ioctlv(fd, 0x6C, 2, 1, iovec);

    freeIobuf(iobuf);
    return ret;
}
