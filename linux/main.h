#pragma once

#include <linux/types.h>
#include <linux/timer.h>
#include <linux/usb/gadget.h>
#include "../common/common.h"

typedef struct {
    // the usb gadget associated with this device
    struct usb_gadget* gadget;
    // request buffer for ep0
    struct usb_request* ep0_request;
    // timer for the state machine
    struct timer_list state_timer;
    // the common device
    udpih_device_t common_dev;
} udpih_linux_device_t;

/** utils **/

struct usb_request* alloc_ep_request(struct usb_ep* ep, u32 length);

void free_ep_request(struct usb_ep* ep, struct usb_request* request);
