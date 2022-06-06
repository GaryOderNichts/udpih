#pragma once

#include "common.h"

typedef struct {
    uint32_t magic;
    uint32_t size;
    be32ptr_t prev;
    be32ptr_t next;
} HeapBlockHeader;
CHECK_SIZE(HeapBlockHeader, 0x10);

typedef struct {
    uint32_t index;
    /* Queue* */ be32ptr_t queue;
    /* QueueItemHeader* */ be32ptr_t prev;
    /* QueueItemHeader* */ be32ptr_t next;
} QueueItemHeader;
CHECK_SIZE(QueueItemHeader, 0x10);

typedef struct {
    uint32_t numItems;
    /* QueueItemHeader* */ be32ptr_t first;
    /* QueueItemHeader* */ be32ptr_t last;
} Queue;
CHECK_SIZE(Queue, 0xc);

typedef struct __attribute__((packed)) {
    QueueItemHeader header;
    /* LocalEventHandler* */ be32ptr_t handler;
    uint64_t timeout;
    uint32_t flags;
} LocalEvent;
CHECK_SIZE(LocalEvent, 0x20);

typedef struct {
    be32ptr_t callback;
    Queue freeQueue;
    uint32_t event_size;
    uint32_t num_events;
    be32ptr_t events_buf;
} LocalEventHandler;
CHECK_SIZE(LocalEventHandler, 0x1c);

typedef struct {
    QueueItemHeader header;
    uint8_t bmRequestType;
    uint8_t bRequest;
    uint16_t wValue;
    uint16_t wIndex;
    uint16_t wLength;
    be32ptr_t buffer;
    be32ptr_t argptr;
    uint32_t timeout;
    int32_t result;
    be32ptr_t callback;
} ControlTransactionEvent;
CHECK_SIZE(ControlTransactionEvent, 0x2c);

typedef struct {
    LocalEvent event;
    be32ptr_t unk;
    /* UhsCtrlXferMgr* */ be32ptr_t mgr;
    uint32_t xferEvent;
    be32ptr_t arg;
} CtrlXferFsmLocalEvent;
CHECK_SIZE(CtrlXferFsmLocalEvent, 0x30);

typedef struct {
    uint32_t state;
    uint32_t new_state;
    /* UhsDevice* */ be32ptr_t device;
    Queue xfer_txn_queue;
    Queue control_transaction_queue;
    /* UhsEndpointUrbEvent* */ be32ptr_t cur_urb_event;
    /* ControlTransactionEvent* */ be32ptr_t pending_transaction_event;
    /* ControlTransactionEvent* */ be32ptr_t CtrlXferTxn;
    be32ptr_t CtrlXferSetupBuf;
    LocalEventHandler eventHandler;
    CtrlXferFsmLocalEvent events[32];
} UhsCtrlXferMgr;
CHECK_SIZE(UhsCtrlXferMgr, 0x650);
