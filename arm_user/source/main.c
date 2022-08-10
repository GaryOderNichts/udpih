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

#include "imports.h"

UhsServer* _main()
{
    // fix up the free block header
    HeapBlockHeader* blockHeader = (HeapBlockHeader*) 0x102c0500;
    blockHeader->size = 0x18d360;
    blockHeader->next = NULL;

    UhsDevice* device = (UhsDevice*) 0x102992e0;

    // set some unfreeable blocks to NULL, so device destruction won't fail
    device->UhsDevStrings = NULL;
    device->ctrlXferMgr->CtrlXferTxn = NULL;
    memset(device->config_descriptors, 0, sizeof(device->config_descriptors));

    // fix up ctrlXferMgr state
    device->ctrlXferMgr->state = CTRL_XFER_STATE_IDLE;
    device->ctrlXferMgr->pending_transaction_event = NULL;

    // get the uhsserver
    UhsServer* server;
    UhsServerGet(0, &server);

    // queue a destroy event for the exploit device
    devFsm_send_event(server, device, DEVICE_EVENT_DESTROY, NULL);

    // return server to the background thread
    return server;
}
