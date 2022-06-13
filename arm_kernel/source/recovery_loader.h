/*
 *   Copyright (C) 2022 GaryOderNichts
 *
 *   This program is free software; you can redistribute it and/or modify it
 *   under the terms and conditions of the GNU General Public License,
 *   version 2, as published by the Free Software Foundation.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#pragma once

#include <stdint.h>

// 'REC\0'
#define HEADER_MAGIC 0x52454300

typedef struct {
    uint32_t vaddr;
    uint32_t paddr;
    uint32_t size;
    uint32_t offset;
} Section_t;

typedef struct {
    uint32_t magic;
    uint32_t entry;
    uint32_t numSections;
    Section_t sections[14];
} RecoveryHeader_t;

int load_recovery(void);
