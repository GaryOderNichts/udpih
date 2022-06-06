#pragma once

#include <stdint.h>
#include <assert.h>

#define CHECK_SIZE(type, size) static_assert(sizeof(type) == size, #type " must be " #size " bytes")

typedef struct Queue Queue;
typedef struct QueueItemHeader QueueItemHeader;
typedef struct UhsCtrlXferMgr UhsCtrlXferMgr;
typedef struct UhsDevice UhsDevice;

typedef struct {
    uint32_t magic;
    uint32_t size;
    void* prev;
    void* next;
} HeapBlockHeader;
CHECK_SIZE(HeapBlockHeader, 0x10);

struct QueueItemHeader {
    uint32_t index;
    Queue* queue;
    QueueItemHeader* prev;
    QueueItemHeader* next;
};
CHECK_SIZE(QueueItemHeader, 0x10);

struct Queue {
    uint32_t numItems;
    QueueItemHeader* first;
    QueueItemHeader* last;
};
CHECK_SIZE(Queue, 0xc);

typedef struct {
    void* callback;
    Queue freeQueue;
    uint32_t event_size;
    uint32_t num_events;
    void* events_buf;
} LocalEventHandler;
CHECK_SIZE(LocalEventHandler, 0x1c);

typedef struct __attribute__((packed)) {
    QueueItemHeader header;
    LocalEventHandler* handler;
    uint64_t timeout;
    uint32_t flags;
} LocalEvent;
CHECK_SIZE(LocalEvent, 0x20);

typedef struct __attribute__((packed)) {
    int nodeId;
    int logHandle;
    int heapId;
    uint32_t threadPriority;
    int resourceManager;
    uint32_t unk0;
    uint32_t unk1;
    uint64_t capabilities;
    char unk2[0x38c8];
    uint32_t numConnectedDevices;
    uint32_t unk3;
    uint32_t unk4;
    uint32_t unk5;
    Queue eventQueue;
    Queue timeoutEventQueue;
    char unk6[0xc8];
    uint32_t clt_traces_enabled;
    uint32_t dev_traces_enabled;
    uint32_t ctrl_traces_enabled;
    uint32_t hub_traces_enabled;
    char unk7[0x3d8];
    uint8_t foregroundStack[0x10000];
    uint32_t foregroundMessages[0x400];
    int foregroundThread;
    int foregroundMessageQueue;
    uint32_t unk8;
    uint8_t backgroundStack[0x10000];
    uint32_t backgroundMessages[0x400];
    int backgroundThread;
    int backgroundMessageQueue;
    char unk9[0x3c];
} UhsServer;
CHECK_SIZE(UhsServer, 0x25e14);

typedef enum {
    CTRL_XFER_STATE_INVALID,
    CTRL_XFER_STATE_IDLE,
    CTRL_XFER_STATE_SEND_SETUP,
    CTRL_XFER_STATE_RECV_DATA,
    CTRL_XFER_STATE_SEND_DATA,
} UhsCtrlXferState;

typedef enum {
    CTRL_XFER_EVENT_INVALID,
    CTRL_XFER_EVENT_ENTRY,
    CTRL_XFER_EVENT_EXIT,
    CTRL_XFER_EVENT_QUEUE_CONTROL_XFER,
    CTRL_XFER_EVENT_URB_COMPLETED,
    CTRL_XFER_EVENT_POLL_EVENT,
    CTRL_XFER_EVENT_ERROR,
} UhsCtrlXferEvent;

typedef struct {
    LocalEvent event;
    uint32_t unk;
    UhsCtrlXferMgr* mgr;
    UhsCtrlXferEvent xferEvent;
    void* arg;
} CtrlXferFsmLocalEvent;
CHECK_SIZE(CtrlXferFsmLocalEvent, 0x30);

typedef struct {
    QueueItemHeader header;
    uint8_t bmRequestType;
    uint8_t bRequest;
    uint16_t wValue;
    uint16_t wIndex;
    uint16_t wLength;
    void* buffer;
    void* argptr;
    uint32_t timeout;
    int32_t result;
    void* callback;
} ControlTransactionEvent;
CHECK_SIZE(ControlTransactionEvent, 0x2c);

struct UhsCtrlXferMgr {
    /* UhsCtrlXferState */ uint32_t state;
    /* UhsCtrlXferState */ uint32_t new_state;
    UhsDevice* device;
    Queue xfer_txn_queue;
    Queue control_transaction_queue;
    /* UhsEndpointUrbEvent* */ void* cur_urb_event;
    ControlTransactionEvent* pending_transaction_event;
    ControlTransactionEvent* CtrlXferTxn;
    void* CtrlXferSetupBuf;
    LocalEventHandler eventHandler;
    CtrlXferFsmLocalEvent events[32];
};
CHECK_SIZE(UhsCtrlXferMgr, 0x650);

typedef enum {
    DEVICE_STATE_INVALID,
    DEVICE_STATE_ASSIGN_ADDRESS,
    DEVICE_STATE_IDENTIFY_DEVICE,
    DEVICE_STATE_RETRIEVE_CONFIGS,
    DEVICE_STATE_RETRIEVE_STRINGS,
    DEVICE_STATE_SET_CONFIG,
    DEVICE_STATE_RECOVERY,
    DEVICE_STATE_CONFIGURED,
    DEVICE_STATE_SUSPENDED,
    DEVICE_STATE_DESTROY_ANCESTRY,
    DEVICE_STATE_DESTROY_SELF,
} UhsDeviceState;

typedef enum {
    DEVICE_EVENT_INVALID,
    DEVICE_EVENT_ENTRY,
    DEVICE_EVENT_EXIT,
    DEVICE_EVENT_CTRL_XFER_COMP,
    DEVICE_EVENT_ADMIN_SETIF,
    DEVICE_EVENT_ADMIN_SUSPEND,
    DEVICE_EVENT_ADMIN_RESUME,
    DEVICE_EVENT_ADMIN_EP,
    DEVICE_EVENT_EP_DESTROYED,
    DEVICE_EVENT_NO_DESCENDENTS,
    DEVICE_EVENT_ERROR,
    DEVICE_EVENT_DESTROY,
    DEVICE_EVENT_HUB_TERMINATED,
    DEVICE_EVENT_RECOVERY_OK,
    DEVICE_EVENT_POWER_MANAGEMENT,
} UhsDeviceEvent;

typedef struct {
    LocalEvent local_event;
    uint32_t unk;
    UhsDevice* device;
    uint32_t event;
    uint32_t arg;
} UhsDevLocalEvent;
CHECK_SIZE(UhsDevLocalEvent, 0x30);

struct __attribute__((packed)) UhsDevice {
    /* UhsDeviceState */ uint32_t state;
    /* UhsDeviceState */ uint32_t new_state;
    uint32_t configDescriptorReadCount;
    uint32_t deviceSpeed;
    uint8_t deviceAddress;
    uint8_t deviceAddress2;
    char padding[2];
    uint32_t noHub;
    uint32_t unk0;
    void* UhsDevStrings;
    uint8_t deviceDescriptor[0x12];
    uint8_t foregroundRequest[0x3a];
    LocalEventHandler eventHandler;
    UhsDevLocalEvent events[0x10];
    char unk1[0x464];
    uint32_t endpointMask;
    UhsCtrlXferMgr* ctrlXferMgr;
    int transfer_result;
    uint32_t transfer_size;
    void* pEp0DmaBuf;
    char unk2[0x24];
    void* config_descriptors[0x20];
    char unk3[0x390c];
};
CHECK_SIZE(UhsDevice, 0x41b0);
