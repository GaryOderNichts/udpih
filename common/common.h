#pragma once

#ifdef __linux
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/usb/ch9.h>

#define DEBUG(x, ...) printk(KERN_DEBUG x, ##__VA_ARGS__)
#define INFO(x, ...) printk(KERN_INFO x, ##__VA_ARGS__)
#define WARNING(x, ...) printk(KERN_WARNING x, ##__VA_ARGS__)
#define ERROR(x, ...) printk(KERN_ERR x, ##__VA_ARGS__)

#define SETUP_NO_DATA 0
#else
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <errno.h>
#include <string.h>
#include <assert.h>
#include "usb/ch9.h"

#define DEBUG(x, ...) printf(x, ##__VA_ARGS__)
#define INFO(x, ...) printf(x, ##__VA_ARGS__)
#define WARNING(x, ...) printf(x, ##__VA_ARGS__)
#define ERROR(x, ...) printf(x, ##__VA_ARGS__)

#define cpu_to_be32(x) __builtin_bswap32(x)
#define cpu_to_be16(x) __builtin_bswap16(x)
#define be32_to_cpu(x) __builtin_bswap32(x)
#define be16_to_cpu(x) __builtin_bswap16(x)
#define cpu_to_le16(x) (x)
#define cpu_to_le32(x) (x)
#define le16_to_cpu(x) (x)
#define le32_to_cpu(x) (x)

#define container_of(ptr, type, member) ({                      \
        const typeof( ((type *)0)->member ) *__mptr = (ptr);    \
        (type *)( (char *)__mptr - offsetof(type,member) );})

#define SETUP_NO_DATA -2

#define min(x, y) (((x) < (y)) ? (x) : (y))
#endif

#if defined(static_assert)
#define CHECK_SIZE(type, size) static_assert(sizeof(type) == size, #type " must be " #size " bytes")
#else
#define CHECK_SIZE(type, size)
#endif

typedef uint32_t be32ptr_t;

typedef struct udpih_device udpih_device_t;

typedef void (*SetStateTimerFn)(udpih_device_t* device, uint32_t ms);

enum {
    STATE_INIT,

    STATE_DEVICE0_CONNECTED,
    STATE_DEVICE0_READY,
    STATE_DEVICE0_DISCONNECTED,

    STATE_DEVICE1_CONNECTED,
    STATE_DEVICE1_READY,
    STATE_DEVICE1_DISCONNECTED,

    STATE_DEVICE2_CONNECTED,
};

typedef struct udpih_device {
    // sets the state timer (0 for immediate state change)
    SetStateTimerFn set_state_timer;

    // the current state
    int state;
} udpih_device_t;

int device_bind(udpih_device_t* device, uint8_t maxpacket);

int device_setup(udpih_device_t* device, const struct usb_ctrlrequest* ctrlrequest, uint8_t* buf, bool high_speed);
