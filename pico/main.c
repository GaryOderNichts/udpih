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

#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "usb.h"
#include "common.h"

void set_usb_address(uint8_t address);
void state_handler_set_timer(udpih_device_t *device, uint32_t ms);

static bool pending_address_update = false;
static uint8_t current_usb_address = 0;

// matches UhsDevice's pEp0DmaBuf size
static uint8_t ep0_buffer[0x10000];

static struct usb_ctrlrequest ctrl_xfer_request;

static udpih_device_t device = {
    .set_state_timer = state_handler_set_timer,
    .state = STATE_INIT,
};

void usb_setup_packet_callback(struct usb_ctrlrequest *pkt)
{
    memcpy(&ctrl_xfer_request, pkt, sizeof(ctrl_xfer_request));

    if (device.state == STATE_INIT) {
        device.state = STATE_DEVICE0_CONNECTED;
    }

    int res;
    if (pkt->bRequest == USB_REQ_SET_ADDRESS && pkt->bRequestType == USB_DIR_OUT) {
        uint8_t address = pkt->wValue & 0xff;
        set_usb_address(address);
        res = 0;
    }
    else {
        // let the device handle the packet
        res = device_setup(&device, pkt, ep0_buffer, false);
        if (res < 0 && res != SETUP_NO_DATA) {
            // Stall EP0 on failure
            usb_device_stall_endpoint(EP0_IN_ADDR);
            usb_device_stall_endpoint(EP0_OUT_ADDR);
            return;
        }
    }

    if (res == 0) {
        // send status to the opposite direction of the data stage
        usb_device_start_transfer((pkt->bRequestType & USB_ENDPOINT_DIR_MASK) ? EP0_OUT_ADDR : EP0_IN_ADDR, NULL, 0);
        return;
    }

    // SETUP_NO_DATA means send data of size 0
    res = (res == SETUP_NO_DATA) ? 0 : res;

    // send data
    usb_device_start_transfer((pkt->bRequestType & USB_ENDPOINT_DIR_MASK) ? EP0_IN_ADDR : EP0_OUT_ADDR, ep0_buffer, res);
}

void usb_bus_reset_callback(void)
{
    current_usb_address = 0;
    usb_device_set_address(0);
}

static void ep0_handler(struct usb_endpoint *ep, uint8_t *buf, uint16_t len)
{
    // opposite direction means status stage is complete
    if ((ep->address & USB_ENDPOINT_DIR_MASK) != (ctrl_xfer_request.bRequestType & USB_ENDPOINT_DIR_MASK)) {
        // no data in status stage
        assert(len == 0);

        printf("status complete\n");

        if (pending_address_update) {
            usb_device_set_address(current_usb_address);
            pending_address_update = false;
        }

        return;
    }

    printf("data complete\n");

    // make sure all data was transferred
    assert((ctrl_xfer_request.wLength == len) || (len < ep->wMaxPacketSize));

    // act status
    usb_device_start_transfer((ctrl_xfer_request.bRequestType & USB_ENDPOINT_DIR_MASK) ? EP0_OUT_ADDR : EP0_IN_ADDR, NULL, 0);
}

void set_usb_address(uint8_t address)
{
    if (current_usb_address != address) {
        current_usb_address = address;
        pending_address_update = true;
    }
}

void state_handler(void)
{
    printf("state_handler: handle state %d\n", device.state);

    switch (device.state) {
    case STATE_DEVICE0_READY:
        usb_device_disconnect();
        device.state = STATE_DEVICE0_DISCONNECTED;
        device.set_state_timer(&device, 500);
        break;
    case STATE_DEVICE0_DISCONNECTED:
        usb_device_connect();
        device.state = STATE_DEVICE1_CONNECTED;
        break;
    case STATE_DEVICE1_READY:
        usb_device_disconnect();
        device.state = STATE_DEVICE1_DISCONNECTED;
        device.set_state_timer(&device, 500);
        break;
    case STATE_DEVICE1_DISCONNECTED:
        usb_device_connect();
        device.state = STATE_DEVICE2_CONNECTED;
        break;
    default:
        ERROR("Invalid state %d\n", device.state);
        break;
    }
}

static int64_t state_alarm_handler(alarm_id_t id, void *user_data)
{
    state_handler();
    return 0;
}

void state_handler_set_timer(udpih_device_t *device, uint32_t ms)
{
    if (ms == 0) {
        state_handler();
        return;
    }

    add_alarm_in_ms(ms, state_alarm_handler, NULL, false);
}

int main()
{
    stdio_init_all();
    printf("Wii U UHS exploit\n");

    // init the usb device
    usb_device_init();

    // setup ep0
    usb_device_setup_endpoint(EP0_IN_ADDR, 64, USB_ENDPOINT_XFER_CONTROL, ep0_handler);
    usb_device_setup_endpoint(EP0_OUT_ADDR, 64, USB_ENDPOINT_XFER_CONTROL, ep0_handler);

    device_bind(&device, 64);

    // the rest will be handled by interrupts through the handlers
    // so just loop forever here
    while (true) {
        tight_loop_contents();
    }

    return 0;
}
