/**
 * 
 * 
 * Based on the dev-lowlevel example from the pico-examples:
 * Copyright (c) 2020 Raspberry Pi (Trading) Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include "usb.h"

// Pico
#include "pico/stdlib.h"

#include <stdio.h>
// For memcpy
#include <string.h>

// USB register definitions from pico-sdk
#include "hardware/regs/usb.h"
// USB hardware struct definitions from pico-sdk
#include "hardware/structs/usb.h"
// For interrupt enable and numbers
#include "hardware/irq.h"
// For resetting the USB controller
#include "hardware/resets.h"

#define usb_hw_set hw_set_alias(usb_hw)
#define usb_hw_clear hw_clear_alias(usb_hw)

static struct usb_endpoint endpoints[USB_MAX_ENDPOINTS][2];

static inline uint8_t ep_get_num(uint8_t addr) {
    return addr & USB_ENDPOINT_NUMBER_MASK;
}

static inline bool ep_is_in(uint8_t addr) {
    return (addr & USB_ENDPOINT_DIR_MASK) == USB_DIR_IN;
}

static struct usb_endpoint *usb_get_endpoint_by_num(uint8_t num, bool in) {
    return &endpoints[num][in];
}

static struct usb_endpoint *usb_get_endpoint_by_addr(uint8_t addr) {
    uint8_t num = ep_get_num(addr);
    bool in = ep_is_in(addr);
    return usb_get_endpoint_by_num(num, in);
}

/**
 * @brief Take a buffer pointer located in the USB RAM and return as an offset of the RAM.
 *
 * @param buf
 * @return uint32_t
 */
static inline uint32_t usb_buffer_offset(volatile uint8_t *buf) {
    return (uint32_t) buf ^ (uint32_t) usb_dpram;
}

void usb_device_setup_endpoint(uint8_t ep_address, uint16_t wMaxPacketSize, uint8_t transfer_type, usb_ep_handler handler) {
    // to make allocations easier split the hw buffers in 64 byte chunks
    assert(wMaxPacketSize <= 64);

    struct usb_endpoint *ep = usb_get_endpoint_by_addr(ep_address);
    uint8_t num = ep_get_num(ep_address);

    ep->configured = true;
    ep->handler = handler;

    ep->address = ep_address;
    ep->wMaxPacketSize = wMaxPacketSize;
    ep->transfer_type = transfer_type;

    ep->next_pid = 0;

    if (ep_is_in(ep_address)) {
        ep->buffer_control = &usb_dpram->ep_buf_ctrl[num].in;
    } else {
        ep->buffer_control = &usb_dpram->ep_buf_ctrl[num].out;
    }

    // clear existing buf control
    *ep->buffer_control = 0;

    if (num == 0) {
        // ep0 has no endpoint control register
        ep->endpoint_control = NULL;

        // fixed ep0 buffer
        ep->data_buffer = &usb_dpram->ep0_buf_a[0];
    } else {
        if (ep_is_in(ep_address)) {
            ep->endpoint_control = &usb_dpram->ep_ctrl[num - 1].in;
        } else {
            ep->endpoint_control = &usb_dpram->ep_ctrl[num - 1].out;
        }

        ep->data_buffer = &usb_dpram->epx_data[((num - 1) * 64) 
            + (USB_MAX_ENDPOINTS * 64 * ep_is_in(ep_address))];

        // Get the data buffer as an offset of the USB controller's DPRAM
        uint32_t dpram_offset = usb_buffer_offset(ep->data_buffer);
        assert(dpram_offset <= USB_DPRAM_MAX);

        uint32_t reg = EP_CTRL_ENABLE_BITS
                    | EP_CTRL_INTERRUPT_PER_BUFFER
                    | (transfer_type << EP_CTRL_BUFFER_TYPE_LSB)
                    | dpram_offset;

        *ep->endpoint_control = reg;
    }
}

void usb_device_stall_endpoint(uint8_t ep_address)
{
    // Stalls need to be armed on EP0
    if (ep_get_num(ep_address) == 0) {
        if (ep_is_in(ep_address)) {
            usb_hw->ep_stall_arm = USB_EP_STALL_ARM_EP0_IN_BITS;
        } else {
            usb_hw->ep_stall_arm = USB_EP_STALL_ARM_EP0_OUT_BITS;
        }
    }

    // Stall buffers
    *usb_get_endpoint_by_addr(ep_address)->buffer_control = USB_BUF_CTRL_STALL;
}

/**
 * @brief Set up the USB controller in device mode, clearing any previous state.
 *
 */
void usb_device_init(void) {
    // Reset usb controller
    reset_block(RESETS_RESET_USBCTRL_BITS);
    unreset_block_wait(RESETS_RESET_USBCTRL_BITS);

    // Clear any previous state in dpram just in case
    memset(usb_dpram, 0, sizeof(*usb_dpram)); // <1>

    // Enable USB interrupt at processor
    irq_set_enabled(USBCTRL_IRQ, true);

    // Mux the controller to the onboard usb phy
    usb_hw->muxing = USB_USB_MUXING_TO_PHY_BITS | USB_USB_MUXING_SOFTCON_BITS;

    // Force VBUS detect so the device thinks it is plugged into a host
    usb_hw->pwr = USB_USB_PWR_VBUS_DETECT_BITS | USB_USB_PWR_VBUS_DETECT_OVERRIDE_EN_BITS;

    // Enable the USB controller in device mode.
    usb_hw->main_ctrl = USB_MAIN_CTRL_CONTROLLER_EN_BITS;

    // Enable an interrupt per EP0 transaction
    usb_hw->sie_ctrl = USB_SIE_CTRL_EP0_INT_1BUF_BITS; // <2>

    // Enable interrupts for when a buffer is done, when the bus is reset,
    // and when a setup packet is received
    usb_hw->inte = USB_INTS_BUFF_STATUS_BITS |
                   USB_INTS_BUS_RESET_BITS |
                   USB_INTS_SETUP_REQ_BITS;

    // Present full speed device by enabling pull up on DP
    usb_hw_set->sie_ctrl = USB_SIE_CTRL_PULLUP_EN_BITS;

    memset(endpoints, 0, sizeof(endpoints));
}

static void start_next_buffer(struct usb_endpoint *ep) {
    uint16_t const buflen = MIN(ep->remaining_length, ep->wMaxPacketSize);
    ep->remaining_length -= buflen;

    // Prepare buffer control register value
    uint32_t buffer_control = buflen | USB_BUF_CTRL_AVAIL;

    if (ep_is_in(ep->address)) {
        // Need to copy the data from the user buffer to the usb memory
        memcpy((void *) ep->data_buffer, (void *) ep->user_buffer + ep->transferred_length, buflen);

        // Mark as full
        buffer_control |= USB_BUF_CTRL_FULL;
    }

    // Set pid and flip for next transfer
    buffer_control |= ep->next_pid ? USB_BUF_CTRL_DATA1_PID : USB_BUF_CTRL_DATA0_PID;
    ep->next_pid ^= 1u;

    if (ep->remaining_length == 0) {
        buffer_control |= USB_BUF_CTRL_LAST;
    }

    *ep->buffer_control = buffer_control;
}

/**
 * @brief Continues a transfer on a given endpoint.
 *
 * @param ep, the endpoint.
 * @return true if the transfer completed
 */
static bool usb_continue_transfer(struct usb_endpoint *ep) {
    assert(ep->active);

    uint32_t buffer_control = *ep->buffer_control;

    uint16_t transferred_bytes = buffer_control & USB_BUF_CTRL_LEN_MASK;

    if (ep_is_in(ep->address)) {
        // data was sent successfully, increase transferred length
        assert(!(buffer_control & USB_BUF_CTRL_FULL));

        ep->transferred_length += transferred_bytes;
    } else {
        // some data was received, copy it to the user buffer
        assert(buffer_control & USB_BUF_CTRL_FULL);

        memcpy(ep->user_buffer + ep->transferred_length, (uint8_t *) ep->data_buffer, transferred_bytes);
        ep->transferred_length += transferred_bytes;
    }

    // if this is a short packet, it's the last buffer
    if (transferred_bytes < ep->wMaxPacketSize) {
        ep->remaining_length = 0;
    }

    if (ep->remaining_length == 0) {
        //printf("Completed transfer on ep 0x%x with a size of %d bytes\n", ep->address, ep->transferred_length);
        return true;
    }

    // Continue transfer
    start_next_buffer(ep);
    return false;
}

/**
 * @brief Starts a transfer on a given endpoint.
 *
 * @param ep_addr, the endpoint address.
 * @param buf, the data buffer to send or receive to
 * @param len, the length of the data in buf
 */
void usb_device_start_transfer(uint8_t ep_address, uint8_t *buf, uint16_t len) {
    struct usb_endpoint* ep = usb_get_endpoint_by_addr(ep_address);

    if (ep->active) {
        printf("Warning: ep 0x%x transfer already active\n", ep->address);
    }

    //printf("Start transfer of len %d on ep addr 0x%x\n", len, ep->address);

    ep->active = true;
    ep->transferred_length = 0;
    ep->remaining_length = len;
    ep->user_buffer = buf;

    // start the transfer
    start_next_buffer(ep);
}

/**
 * @brief Handle a BUS RESET from the host
 *
 */
static void usb_bus_reset(void) {
    usb_bus_reset_callback();
}

/**
 * @brief Respond to a setup packet from the host.
 *
 */
static void usb_handle_setup_packet(void) {
    // Reset PID to 1 for EP0 IN and OUT
    usb_get_endpoint_by_num(0, false)->next_pid = 1u;
    usb_get_endpoint_by_num(0, true)->next_pid = 1u;

    volatile struct usb_ctrlrequest *pkt = (volatile struct usb_ctrlrequest *) &usb_dpram->setup_packet;
    usb_setup_packet_callback((struct usb_ctrlrequest *) pkt);
}

/**
 * @brief Handle a "buffer status" irq. This means that one or more
 * buffers have been sent / received. Notify each endpoint where this
 * is the case.
 */
static void usb_handle_buff_status(void) {
    uint32_t buffers = usb_hw->buf_status;
    uint32_t remaining_buffers = buffers;

    uint bit = 1u;
    for (uint i = 0; remaining_buffers && i < USB_NUM_ENDPOINTS * 2; i++) {
        if (remaining_buffers & bit) {
            // clear this in advance
            usb_hw_clear->buf_status = bit;

            // IN transfer for even i, OUT transfer for odd i
            struct usb_endpoint* ep = usb_get_endpoint_by_num(i >> 1u, !(i & 1u));

            // continue the transfer
            if (usb_continue_transfer(ep)) {
                // call the handler if the transfer is done
                if (ep->handler) {
                    ep->handler(ep, ep->user_buffer, ep->transferred_length);
                }

                // transfer is no longer active
                ep->active = false;
            }

            remaining_buffers &= ~bit;
        }
        bit <<= 1u;
    }
}

/**
 * @brief USB interrupt handler
 *
 */
/// \tag::isr_setup_packet[]
void isr_usbctrl(void) {
    // USB interrupt handler
    uint32_t status = usb_hw->ints;
    uint32_t handled = 0;

    // Setup packet received
    if (status & USB_INTS_SETUP_REQ_BITS) {
        handled |= USB_INTS_SETUP_REQ_BITS;
        usb_hw_clear->sie_status = USB_SIE_STATUS_SETUP_REC_BITS;
        usb_handle_setup_packet();
    }
/// \end::isr_setup_packet[]

    // Buffer status, one or more buffers have completed
    if (status & USB_INTS_BUFF_STATUS_BITS) {
        handled |= USB_INTS_BUFF_STATUS_BITS;
        usb_handle_buff_status();
    }

    // Bus is reset
    if (status & USB_INTS_BUS_RESET_BITS) {
        printf("BUS RESET\n");
        handled |= USB_INTS_BUS_RESET_BITS;
        usb_hw_clear->sie_status = USB_SIE_STATUS_BUS_RESET_BITS;
        usb_bus_reset();
    }

    if (status ^ handled) {
        panic("Unhandled IRQ 0x%x\n", (uint) (status ^ handled));
    }
}

void usb_device_set_address(uint8_t address) {
    printf("Setting address to %x\n", address);

    usb_hw->dev_addr_ctrl = address;
}

void usb_device_disconnect(void) {
    usb_hw_clear->sie_ctrl = USB_SIE_CTRL_PULLUP_EN_BITS;
}

void usb_device_connect(void) {
    usb_hw_set->sie_ctrl = USB_SIE_CTRL_PULLUP_EN_BITS;
}
