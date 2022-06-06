#pragma once

#include <stdint.h>
#include "pico/stdlib.h"

#include "usb/ch9.h"

#define EP0_IN_ADDR (USB_DIR_IN | 0)
#define EP0_OUT_ADDR (USB_DIR_OUT | 0)

struct usb_endpoint;
typedef void (*usb_ep_handler)(struct usb_endpoint *ep, uint8_t *buf, uint16_t len);

struct usb_endpoint {
    // is this struct configured
    bool configured;

    // handler that will be called once a transfer completes
    usb_ep_handler handler;

    // the address of this endpoint
    uint8_t address;
    // the maximum packet size of this endpoint
    uint16_t wMaxPacketSize;
    // the transfer type of this endpoint
    uint8_t transfer_type;

    // is a transfer active
    bool active;
    // the remaining length of the transfer
    uint16_t remaining_length;
    // the transferred length
    uint16_t transferred_length;
    // buffer provided by the user
    uint8_t *user_buffer;

    // Toggle after each packet (unless replying to a SETUP)
    uint8_t next_pid;

    // Pointers to endpoint + buffer control registers
    // in the USB controller DPSRAM
    volatile uint32_t *endpoint_control;
    volatile uint32_t *buffer_control;
    volatile uint8_t *data_buffer;
};

void usb_device_init(void);

void usb_device_setup_endpoint(uint8_t ep_address, uint16_t wMaxPacketSize, uint8_t transfer_type, usb_ep_handler handler);

void usb_device_stall_endpoint(uint8_t ep_address);

void usb_device_start_transfer(uint8_t ep_address, uint8_t *buf, uint16_t len);

void usb_device_set_address(uint8_t address);

void usb_device_disconnect(void);

void usb_device_connect(void);

// has to be implemented by the user
void usb_setup_packet_callback(struct usb_ctrlrequest *pkt);

// has to be implemented by the user
void usb_bus_reset_callback(void);
