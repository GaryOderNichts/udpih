#include "main.h"

struct usb_request* alloc_ep_request(struct usb_ep* ep, u32 length)
{
    struct usb_request* request = usb_ep_alloc_request(ep, GFP_ATOMIC);
    if (!request) {
        return NULL;
    }

    request->length = length;

    request->buf = kmalloc(length, GFP_ATOMIC);
    if (!request->buf) {
        usb_ep_free_request(ep, request);
        return NULL;
    }

    return request;
}

void free_ep_request(struct usb_ep* ep, struct usb_request* request)
{
    if (request) {
        kfree(request->buf);
        usb_ep_free_request(ep, request);
    }
}
