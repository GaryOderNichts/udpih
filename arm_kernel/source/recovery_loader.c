/*
 *   Copyright (C) 2022 GaryOderNichts
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "recovery_loader.h"
#include "fsa.h"

#define READ_BUFFER_SIZE 0x400

static int load_segment(int fsaHandle, int fileHandle, void* readBuffer, Section_t* section)
{
    int res = FSA_SetPosFile(fsaHandle, fileHandle, section->offset);
    if (res < 0) {
        return res;
    }

    uint32_t bytesRead = 0;
    while (bytesRead < section->size) {
        uint32_t toRead = section->size - bytesRead;
        res = FSA_ReadFile(fsaHandle, readBuffer, 1, (toRead < READ_BUFFER_SIZE) ? toRead : READ_BUFFER_SIZE, fileHandle, 0);
        if (res <= 0) {
            return res;
        }

        void* srcBuffer = IOS_VirtToPhys(readBuffer);
        void* dstBuffer = (void*) (section->paddr + bytesRead);

        // TODO what actually needs to be flushed/invalidated here?
        flush_dcache(readBuffer, READ_BUFFER_SIZE);
        invalidate_dcache(NULL, 0x4001);

        // copy to dst with disabled mmu
        int level = disable_interrupts();
        uint32_t control_register = disable_mmu();

        memcpy(dstBuffer, srcBuffer, res);

        restore_mmu(control_register);
        enable_interrupts(level);

        bytesRead += res;
    }

    return (res >= 0) ? bytesRead : res;
}

int load_recovery(void)
{
    int res = -1;
    int fsaHandle = -1;
    int fileHandle = -1;
    void* readBuffer = NULL;
    RecoveryHeader_t header;

    fsaHandle = IOS_Open("/dev/fsa", 0);
    if (fsaHandle < 0) {
        return fsaHandle;
    }

    res = FSA_Mount(fsaHandle, "/dev/sdcard01", "/vol/storage_udpihsd", 2, NULL, 0);
    if (res < 0) {
        goto error;
    }

    res = FSA_OpenFile(fsaHandle, "/vol/storage_udpihsd/recovery_menu", "rb", &fileHandle);
    if (res < 0) {
        goto error;
    }

    readBuffer = IOS_HeapAllocAligned(0xcaff, READ_BUFFER_SIZE, 0x40);
    if (!readBuffer) {
        res = -1;
        goto error;
    }

    res = FSA_ReadFile(fsaHandle, readBuffer, 1, sizeof(RecoveryHeader_t), fileHandle, 0);
    if (res != sizeof(RecoveryHeader_t)) {
        if (res >= 0) {
            res = -1;
        }
        goto error;
    }

    memcpy(&header, readBuffer, sizeof(RecoveryHeader_t));

    if (header.magic != HEADER_MAGIC) {
        res = -1;
        goto error;
    }

    if (header.numSections > 14) {
        res = -1;
        goto error;
    }

    for (uint32_t i = 0; i < header.numSections; ++i) {
        res = load_segment(fsaHandle, fileHandle, readBuffer, &header.sections[i]);
        if (res < 0) {
            goto error;
        }
    }

    // invalidate all cache
    invalidate_dcache(NULL, 0x4001);
    invalidate_icache();

error: ;

    if (fileHandle > 0) {
        FSA_CloseFile(fsaHandle, fileHandle);
    }

    if (readBuffer) {
        IOS_HeapFree(0xcaff, readBuffer);
    }

    FSA_Unmount(fsaHandle, "/vol/storage_udpihsd", 2);

    IOS_Close(fsaHandle);

    // call the entrypoint
    if (res >= 0) {
        // jump to the entry
        ((void (*)(const char*)) header.entry)("udpih");
    }

    return (res >= 0) ? 0 : res;
}
