/*
 *   Copyright (C) 2025 GaryOderNichts
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
#include <inttypes.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/timers.h"
#include "esp_chip_info.h"
#include "esp_flash.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_check.h"
#include "esp_err.h"
#include "esp_private/periph_ctrl.h"
#include "esp_private/usb_phy.h"
#include "soc/usb_pins.h" // TODO: Will be removed in IDF v6.0 IDF-9029
#include "driver/gpio.h"

#include "common.h"
#include "usb_esp32sx.h"

enum QueueEventType {
    QUEUE_EVENT_SETUP_PACKET,
    QUEUE_EVENT_XFER_COMPLETE,
};

struct QueueEvent {
    enum QueueEventType ev;
    union {
        struct usb_ctrlrequest ctrlrequest;
        struct {
            uint8_t ep_address;
            uint16_t len;
        } xfer;
    };
};

const static char *TAG = "UDPIH";

// PHY handle
static usb_phy_handle_t phy_hdl;

// Global queue
static QueueHandle_t queue;

// matches UhsDevice's pEp0DmaBuf size
static uint8_t ep0_buffer[0x10000];

static struct usb_ctrlrequest ctrl_xfer_request;

#ifdef CONFIG_BLINK_ENABLE
#include "led_strip.h"
static led_strip_handle_t led_strip;
static TimerHandle_t heartbeatTimer;
#endif

void state_handler_set_timer(udpih_device_t *device, uint32_t ms);

static udpih_device_t device = {
    .set_state_timer = state_handler_set_timer,
    .state = STATE_INIT,
};

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

static void state_alarm_callback(TimerHandle_t timer)
{
    xTimerDelete(timer, 0);
    state_handler();
}

void state_handler_set_timer(udpih_device_t *device, uint32_t ms)
{
    if (ms == 0) {
        state_handler();
        return;
    }

    TimerHandle_t timer = xTimerCreate("Timer", pdMS_TO_TICKS(ms), pdFALSE, NULL, state_alarm_callback);
    if (!timer) {
        ESP_LOGE(TAG, "Failed to create timer");
        assert(false);
    }

    xTimerStart(timer, 0);
}

void usb_setup_packet_callback(struct usb_ctrlrequest *pkt)
{
    ESP_EARLY_LOGV(TAG, "Got setup packet with request %02x", pkt->bRequest);

    if (device.state == STATE_INIT) {
        device.state = STATE_DEVICE0_CONNECTED;
    }

    memcpy(&ctrl_xfer_request, pkt, sizeof(ctrl_xfer_request));

    // This functions runs within an interrupt so keep it fast and handle the request in the main function
    struct QueueEvent event;
    event.ev = QUEUE_EVENT_SETUP_PACKET;
    memcpy(&event.ctrlrequest, pkt, sizeof(ctrl_xfer_request));
    if (xQueueSend(queue, &event, (TickType_t) 0) != pdTRUE) {
        ESP_EARLY_LOGE(TAG, "Failed to send event to queue");
        assert(false);
    }
}

void usb_xfer_complete_callback(uint8_t ep_address, uint16_t len)
{
    ESP_EARLY_LOGV(TAG, "usb_xfer_complete_callback %02x %x", ep_address, len);

    // This functions runs within an interrupt so keep it fast and handle the request in the main function
    struct QueueEvent event;
    event.ev = QUEUE_EVENT_XFER_COMPLETE;
    event.xfer.ep_address = ep_address;
    event.xfer.len = len;
    if (xQueueSend(queue, &event, (TickType_t) 0) != pdTRUE) {
        ESP_EARLY_LOGE(TAG, "Failed to send event to queue");
        assert(false);
    }
}

esp_err_t usb_phy_install(void)
{
    // Configure USB PHY
    usb_phy_config_t phy_conf = {
        .controller = USB_PHY_CTRL_OTG,
        .target = USB_PHY_TARGET_INT,
        .otg_mode = USB_OTG_MODE_DEVICE,
#if (USB_PHY_SUPPORTS_P4_OTG11)
        .otg_speed = USB_PHY_SPEED_FULL, // UDPIH only supports full speed
#else
#if (CONFIG_IDF_TARGET_ESP32P4 && CONFIG_TINYUSB_RHPORT_FS)
#error "USB PHY for OTG1.1 is not supported, please update your esp-idf."
#endif // IDF_TARGET_ESP32P4 && CONFIG_TINYUSB_RHPORT_FS
#endif // USB_PHY_SUPPORTS_P4_OTG11
    };

    ESP_RETURN_ON_ERROR(usb_new_phy(&phy_conf, &phy_hdl), TAG, "Install USB PHY failed");

    ESP_LOGI(TAG, "USB Driver installed");
    return ESP_OK;
}

#if CONFIG_BLINK_ENABLE
static void heartbeat_callback(TimerHandle_t timer)
{
    // Invert the led
    static bool led_on = true;
    led_on = !led_on;

    uint32_t blink_speed = 100;

    switch (device.state) {
        case STATE_INIT:
            // Solid white color when no device connected
            led_strip_set_pixel(led_strip, 0, 4, 4, 4);
            led_strip_refresh(led_strip);
            break;
        case STATE_DEVICE0_CONNECTED:
        case STATE_DEVICE0_READY:
        case STATE_DEVICE0_DISCONNECTED:
            // Slow yellow blinking in stage 0
            blink_speed = 300;
            if (led_on) {
                led_strip_set_pixel(led_strip, 0, 4, 4, 0);
                led_strip_refresh(led_strip);
            } else {
                led_strip_clear(led_strip);
            }
            break;
        case STATE_DEVICE1_CONNECTED:
        case STATE_DEVICE1_READY:
        case STATE_DEVICE1_DISCONNECTED:
            // Fast yellow blinking in stage 0
            blink_speed = 100;
            if (led_on) {
                led_strip_set_pixel(led_strip, 0, 4, 4, 0);
                led_strip_refresh(led_strip);
            } else {
                led_strip_clear(led_strip);
            }
            break;
        case STATE_DEVICE2_CONNECTED:
            // Solid green color on completion
            led_strip_set_pixel(led_strip, 0, 0, 4, 0);
            led_strip_refresh(led_strip);
            break;
    }

    // Set new period
    xTimerChangePeriod(timer, pdMS_TO_TICKS(blink_speed), 0);
}
#endif

void app_main(void)
{
    printf("Hello World!\n");

    /* Print chip information */
    esp_chip_info_t chip_info;
    esp_chip_info(&chip_info);
    printf("This is %s chip with %d CPU core(s), %s%s%s%s, ",
           CONFIG_IDF_TARGET,
           chip_info.cores,
           (chip_info.features & CHIP_FEATURE_WIFI_BGN) ? "WiFi/" : "",
           (chip_info.features & CHIP_FEATURE_BT) ? "BT" : "",
           (chip_info.features & CHIP_FEATURE_BLE) ? "BLE" : "",
           (chip_info.features & CHIP_FEATURE_IEEE802154) ? ", 802.15.4 (Zigbee/Thread)" : "");

    unsigned major_rev = chip_info.revision / 100;
    unsigned minor_rev = chip_info.revision % 100;
    printf("silicon revision v%d.%d\n", major_rev, minor_rev);

#if CONFIG_BLINK_ENABLE
    // Set up the LED
    led_strip_config_t strip_config = {
        .strip_gpio_num = CONFIG_BLINK_GPIO,
        .max_leds = 1, // at least one LED on board
    };

    led_strip_rmt_config_t rmt_config = {
        .resolution_hz = 10 * 1000 * 1000, // 10MHz
        .flags.with_dma = false,
    };
    ESP_ERROR_CHECK(led_strip_new_rmt_device(&strip_config, &rmt_config, &led_strip));

    led_strip_clear(led_strip);

    heartbeatTimer = xTimerCreate("HeartbeatTimer", pdMS_TO_TICKS(100), pdFALSE, NULL, heartbeat_callback);
    if (!heartbeatTimer) {
        ESP_LOGE(TAG, "Failed to create heartbeat timer");
        return;
    }

    xTimerStart(heartbeatTimer, 0);
#endif

    queue = xQueueCreate(2, sizeof(struct QueueEvent));
    if (queue == NULL) {
        ESP_LOGE(TAG, "Failed to create event queue");
        return;
    }

    // Bind UDPIH device with max packet size
    device_bind(&device, 64u);

    // Setup USB PHY
    if (usb_phy_install() != ESP_OK) {
        ESP_LOGE(TAG, "Failed to setup USB PHY");
        return;
    }

    // Init USB device and connect
    usb_device_init();

    // Everything USB is interrupt driven, but keep code in interrupts low
    // handle everything else in this queue 
    for (;;) {
        struct QueueEvent event;
        if (xQueueReceive(queue, &event, portMAX_DELAY) == pdPASS) {
            ESP_LOGV(TAG, "got event %02x", event.ev);

            switch (event.ev) {
                case QUEUE_EVENT_SETUP_PACKET: {
                    struct usb_ctrlrequest *pkt = &ctrl_xfer_request;

                    // Handle set address requests
                    if (pkt->bRequest == USB_REQ_SET_ADDRESS && pkt->bRequestType == USB_DIR_OUT) {
                        // The status will also be handled by this function
                        usb_device_set_address(pkt->wValue & 0xff);
                        break;
                    }

                    // let the device handle the packet
                    int res = device_setup(&device, pkt, ep0_buffer, false);
                    if (res < 0 && res != SETUP_NO_DATA) {
                        // Stall EP0 on failure
                        // (this seems somewhat broken on ESP32? Linux reports EPIPE instead of ENOENT for missing descriptors)
                        usb_device_stall_endpoint(EP0_IN_ADDR);
                        usb_device_stall_endpoint(EP0_OUT_ADDR);
                        ESP_LOGW(TAG, "Endpoint stalled");
                        // assert(false);
                        break;
                    }

                    if (res == 0) {
                        // send status to the opposite direction of the data stage
                        usb_device_start_transfer((pkt->bRequestType & USB_ENDPOINT_DIR_MASK) ? EP0_OUT_ADDR : EP0_IN_ADDR, NULL, 0);
                        break;
                    }
                
                    // SETUP_NO_DATA means send data of size 0
                    res = (res == SETUP_NO_DATA) ? 0 : res;
                
                    // send data
                    usb_device_start_transfer((pkt->bRequestType & USB_ENDPOINT_DIR_MASK) ? EP0_IN_ADDR : EP0_OUT_ADDR, ep0_buffer, res);
                    break;
                }
                case QUEUE_EVENT_XFER_COMPLETE: {
                    // opposite direction means status stage is complete
                    if ((event.xfer.ep_address & USB_ENDPOINT_DIR_MASK) != (ctrl_xfer_request.bRequestType & USB_ENDPOINT_DIR_MASK)) {
                        // no data in status stage
                        assert(event.xfer.len == 0);

                        ESP_LOGV(TAG, "status complete");

                        break;
                    }

                    ESP_LOGV(TAG, "data complete");

                    // act status
                    usb_device_start_transfer((ctrl_xfer_request.bRequestType & USB_ENDPOINT_DIR_MASK) ? EP0_OUT_ADDR : EP0_IN_ADDR, NULL, 0);
                    break;
                }
            }
        }
    }
}
