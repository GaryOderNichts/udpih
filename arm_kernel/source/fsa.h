#pragma once
#include "imports.h"

typedef struct {
    uint32_t flags;
    uint32_t mode;
    uint32_t owner;
    uint32_t group;
    uint32_t size;
    uint32_t allocSize;
    uint32_t unk[3];
    uint32_t id;
    uint32_t ctime;
    uint32_t mtime;
    uint32_t unk2[0x0D];
} FSStat;

typedef struct {
    FSStat stat;
    char name[0x100];
} FSDirectoryEntry;

#define DIR_ENTRY_IS_DIRECTORY      0x80000000

#define FSA_MOUNTFLAGS_BINDMOUNT    (1 << 0)
#define FSA_MOUNTFLAGS_GLOBAL       (1 << 1)

int FSA_Mount(int fd, const char* device_path, char* volume_path, uint32_t flags, char* arg_string, int arg_string_len);
int FSA_Unmount(int fd, const char* path, uint32_t flags);
int FSA_FlushVolume(int fd, const char* volume_path);

int FSA_GetDeviceInfo(int fd, const char* device_path, int type, uint32_t* out_data);

int FSA_MakeDir(int fd, const char* path, uint32_t flags);
int FSA_OpenDir(int fd, const char* path, int* outHandle);
int FSA_ReadDir(int fd, int handle, FSDirectoryEntry* out_data);
int FSA_RewindDir(int fd, int handle);
int FSA_CloseDir(int fd, int handle);
int FSA_ChangeDir(int fd, const char* path);

int FSA_OpenFile(int fd, const char* path, const char* mode, int* outHandle);
int FSA_ReadFile(int fd, void* data, uint32_t size, uint32_t cnt, int fileHandle, uint32_t flags);
int FSA_WriteFile(int fd, void* data, uint32_t size, uint32_t cnt, int fileHandle, uint32_t flags);
int FSA_StatFile(int fd, int handle, FSStat* out_data);
int FSA_CloseFile(int fd, int fileHandle);
int FSA_SetPosFile(int fd, int fileHandle, uint32_t position);
int FSA_GetStat(int fd, const char* path, FSStat* out_data);
int FSA_Remove(int fd, const char* path);
int FSA_ChangeMode(int fd, const char* path, int mode);

int FSA_RawOpen(int fd, const char* device_path, int* outHandle);
int FSA_RawRead(int fd, void* data, uint32_t size_bytes, uint32_t cnt, uint64_t sector_offset, int device_handle);
int FSA_RawWrite(int fd, void* data, uint32_t size_bytes, uint32_t cnt, uint64_t sector_offset, int device_handle);
int FSA_RawClose(int fd, int device_handle);
