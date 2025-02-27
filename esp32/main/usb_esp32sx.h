#pragma once

#include <stdint.h>

#include "usb/ch9.h"

#define EP0_IN_ADDR (USB_DIR_IN | 0)
#define EP0_OUT_ADDR (USB_DIR_OUT | 0)

void usb_device_init(void);

void usb_device_set_address(uint8_t address);

void usb_device_disconnect(void);

void usb_device_connect(void);

void usb_device_start_transfer(uint8_t ep_address, uint8_t *buf, uint16_t total_len);

void usb_device_stall_endpoint(uint8_t ep_address);

// has to be implemented by the user
void usb_setup_packet_callback(struct usb_ctrlrequest *pkt);

// has to be implemented by the user
void usb_xfer_complete_callback(uint8_t ep_address, uint16_t len);
