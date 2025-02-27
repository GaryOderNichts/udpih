// This file is from the TinyUSB stack and was adapted for the use with UDPIH.
/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2018 Scott Shawcroft, 2019 William D. Jones for Adafruit Industries
 * Copyright (c) 2019 Ha Thach (tinyusb.org)
 * Additions Copyright (c) 2020, Espressif Systems (Shanghai) Co. Ltd.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 * This file is part of the TinyUSB stack.
 */
#include "usb_esp32sx.h"

#include "esp_intr_alloc.h"
#include "esp_log.h"
#include "soc/dport_reg.h"
#include "soc/gpio_sig_map.h"
#include "soc/usb_periph.h"
#include "soc/usb_reg.h"
#include "soc/usb_struct.h"
#include "soc/periph_defs.h" // for interrupt source

#ifndef USB_OUT_EP_NUM
#define USB_OUT_EP_NUM ((int) (sizeof(USB0.out_ep_reg) / sizeof(USB0.out_ep_reg[0])))
#endif

#ifndef USB_IN_EP_NUM
#define USB_IN_EP_NUM ((int) (sizeof(USB0.in_ep_reg) / sizeof(USB0.in_ep_reg[0])))
#endif

// Max number of bi-directional endpoints including EP0
// Note: ESP32S2 specs say there are only up to 5 IN active endpoints include EP0
// We should probably prohibit enabling Endpoint IN > 4 (not done yet)
#define EP_MAX            USB_OUT_EP_NUM

// FIFO size in bytes
#define EP_FIFO_SIZE      1024

// Max number of IN EP FIFOs
#define EP_FIFO_NUM 5

// Tinyusb directions
#define TUSB_DIR_OUT 0
#define TUSB_DIR_IN  1

typedef struct {
    uint8_t *buffer;
    uint16_t total_len;
    uint16_t queued_len;
    uint16_t max_size;
    bool short_packet;
    uint8_t interval;
} xfer_ctl_t;

// UDPIH edit:
typedef struct {
    uint8_t *buffer;
    uint16_t total_len;
    uint16_t transferred_len;
} ctrl_xfer_ctl_t;

static inline uint8_t ep_get_num(uint8_t addr)
{
    return addr & USB_ENDPOINT_NUMBER_MASK;
}

static inline uint8_t ep_get_dir(uint8_t addr)
{
    return (addr & USB_ENDPOINT_DIR_MASK) == USB_DIR_IN ? TUSB_DIR_IN : TUSB_DIR_OUT;
}

static void _dcd_int_handler(void *arg);

static const char *TAG = "UDPIH:USB";
static intr_handle_t usb_ih;

static uint32_t _setup_packet[2];

#define XFER_CTL_BASE(_ep, _dir) &xfer_status[_ep][_dir]
static xfer_ctl_t xfer_status[EP_MAX][2];
static ctrl_xfer_ctl_t control_xfer_status;

// Setup the control endpoint 0.
static void bus_reset(void)
{
    for (int ep_num = 0; ep_num < USB_OUT_EP_NUM; ep_num++) {
        USB0.out_ep_reg[ep_num].doepctl |= USB_DO_SNAK0_M; // DOEPCTL0_SNAK
    }

    // clear device address
    USB0.dcfg &= ~USB_DEVADDR_M;

    USB0.daintmsk = USB_OUTEPMSK0_M | USB_INEPMSK0_M;
    USB0.doepmsk  = USB_SETUPMSK_M | USB_XFERCOMPLMSK;
    USB0.diepmsk  = USB_TIMEOUTMSK_M | USB_DI_XFERCOMPLMSK_M /*| USB_INTKNTXFEMPMSK_M*/;

    // "USB Data FIFOs" section in reference manual
    // Peripheral FIFO architecture
    //
    // --------------- 320 or 1024 ( 1280 or 4096 bytes )
    // | IN FIFO MAX |
    // ---------------
    // |    ...      |
    // --------------- y + x + 16 + GRXFSIZ
    // | IN FIFO 2   |
    // --------------- x + 16 + GRXFSIZ
    // | IN FIFO 1   |
    // --------------- 16 + GRXFSIZ
    // | IN FIFO 0   |
    // --------------- GRXFSIZ
    // | OUT FIFO    |
    // | ( Shared )  |
    // --------------- 0
    //
    // According to "FIFO RAM allocation" section in RM, FIFO RAM are allocated as follows (each word 32-bits):
    // - Each EP IN needs at least max packet size, 16 words is sufficient for EP0 IN
    //
    // - All EP OUT shared a unique OUT FIFO which uses
    //   * 10 locations in hardware for setup packets + setup control words (up to 3 setup packets).
    //   * 2 locations for OUT endpoint control words.
    //   * 16 for largest packet size of 64 bytes. ( TODO Highspeed is 512 bytes)
    //   * 1 location for global NAK (not required/used here).
    //   * It is recommended to allocate 2 times the largest packet size, therefore
    //   Recommended value = 10 + 1 + 2 x (16+2) = 47 --> Let's make it 52
    USB0.grstctl |= 0x10 << USB_TXFNUM_S; // fifo 0x10,
    USB0.grstctl |= USB_TXFFLSH_M;        // Flush fifo
    USB0.grxfsiz = 52;

    // Control IN uses FIFO 0 with 64 bytes ( 16 32-bit word )
    USB0.gnptxfsiz = (16 << USB_NPTXFDEP_S) | (USB0.grxfsiz & 0x0000ffffUL);

    // Ready to receive SETUP packet
    USB0.out_ep_reg[0].doeptsiz |= USB_SUPCNT0_M;

    USB0.gintmsk |= USB_IEPINTMSK_M | USB_OEPINTMSK_M;
}

static void enum_done_processing(void)
{
    ESP_EARLY_LOGV(TAG, "dcd_int_handler - Speed enumeration done! Sending DCD_EVENT_BUS_RESET then");
    // On current silicon on the Full Speed core, speed is fixed to Full Speed.
    // However, keep for debugging and in case Low Speed is ever supported.
    uint32_t enum_spd = (USB0.dsts >> USB_ENUMSPD_S) & (USB_ENUMSPD_V);

    // Maximum packet size for EP 0 is set for both directions by writing DIEPCTL
    if (enum_spd == 0x03) { // Full-Speed (PHY on 48 MHz)
        USB0.in_ep_reg[0].diepctl &= ~USB_D_MPS0_V;    // 64 bytes
        USB0.in_ep_reg[0].diepctl &= ~USB_D_STALL0_M; // clear Stall
        xfer_status[0][TUSB_DIR_OUT].max_size = 64;
        xfer_status[0][TUSB_DIR_IN].max_size = 64;
    } else {
        // UDPIH edit: UDPIH only supports full-speed, no low speed
        assert(false);

        USB0.in_ep_reg[0].diepctl |= USB_D_MPS0_V;     // 8 bytes
        USB0.in_ep_reg[0].diepctl &= ~USB_D_STALL0_M; // clear Stall
        xfer_status[0][TUSB_DIR_OUT].max_size = 8;
        xfer_status[0][TUSB_DIR_IN].max_size = 8;
    }
}

void usb_device_init(void)
{
    ESP_LOGV(TAG, "DCD init - Start");
  
    // A. Disconnect
    ESP_LOGV(TAG, "DCD init - Soft DISCONNECT and Setting up");
    USB0.dctl |= USB_SFTDISCON_M; // Soft disconnect
  
    // B. Programming DCFG
    /* If USB host misbehaves during status portion of control xfer
      (non zero-length packet), send STALL back and discard. Full speed. */
    USB0.dcfg |= USB_NZSTSOUTHSHK_M | // NonZero .... STALL
        (3 << 0);            // dev speed: fullspeed 1.1 on 48 mhz  // TODO no value in usb_reg.h (IDF-1476)
  
    USB0.gahbcfg |= USB_NPTXFEMPLVL_M | USB_GLBLLNTRMSK_M; // Global interruptions ON
    USB0.gusbcfg |= USB_FORCEDEVMODE_M;                    // force devmode
    USB0.gotgctl &= ~(USB_BVALIDOVVAL_M | USB_BVALIDOVEN_M | USB_VBVALIDOVVAL_M); //no overrides
  
    // C. Setting SNAKs, then connect
    for (int n = 0; n < USB_OUT_EP_NUM; n++) {
        USB0.out_ep_reg[n].doepctl |= USB_DO_SNAK0_M; // DOEPCTL0_SNAK
    }
  
    // D. Interruption masking
    USB0.gintmsk = 0;   //mask all
    USB0.gotgint = ~0U; //clear OTG ints
    USB0.gintsts = ~0U; //clear pending ints
    USB0.gintmsk = USB_OTGINTMSK_M   |
                   USB_MODEMISMSK_M  |
                   USB_RXFLVIMSK_M   |
                   USB_ERLYSUSPMSK_M |
                   USB_USBSUSPMSK_M  |
                   USB_USBRSTMSK_M   |
                   USB_ENUMDONEMSK_M |
                   USB_RESETDETMSK_M |
                   USB_WKUPINT_M |
                   USB_DISCONNINTMSK_M; // host most only

    // Connect the device
    usb_device_connect();

    // Setup interrupt handler
    esp_intr_alloc(ETS_USB_INTR_SOURCE, ESP_INTR_FLAG_LOWMED, (intr_handler_t) _dcd_int_handler, NULL, &usb_ih);
}

void usb_device_set_address(uint8_t address)
{
    ESP_LOGV(TAG, "DCD init - Set address : %u", address);
    USB0.dcfg |= ((address & USB_DEVADDR_V) << USB_DEVADDR_S);

    // Response with status after changing device address
    usb_device_start_transfer(0x80, NULL, 0);
}

void usb_device_disconnect(void)
{
    USB0.dctl |= USB_SFTDISCON_M;
}

void usb_device_connect(void)
{
    USB0.dctl &= ~USB_SFTDISCON_M;
}

void usb_device_xfer(uint8_t ep_address, uint8_t *buf, uint16_t total_len)
{
    uint8_t const epnum = ep_get_num(ep_address);
    uint8_t const dir   = ep_get_dir(ep_address);

    xfer_ctl_t *xfer = XFER_CTL_BASE(epnum, dir);
    xfer->buffer = buf;
    xfer->total_len = total_len;
    xfer->queued_len = 0;
    xfer->short_packet = false;

    uint16_t num_packets = (total_len / xfer->max_size);
    uint8_t short_packet_size = total_len % xfer->max_size;

    // Zero-size packet is special case.
    if (short_packet_size > 0 || (total_len == 0)) {
        num_packets++;
    }

    ESP_LOGV(TAG, "Transfer <-> EP%i, %s, pkgs: %i, bytes: %i",
            epnum, ((dir == TUSB_DIR_IN) ? "USB0.HOST (in)" : "HOST->DEV (out)"),
            num_packets, total_len);

    // IN and OUT endpoint xfers are interrupt-driven, we just schedule them
    // here.
    if (dir == TUSB_DIR_IN) {
        // A full IN transfer (multiple packets, possibly) triggers XFRC.
        USB0.in_ep_reg[epnum].dieptsiz = (num_packets << USB_D_PKTCNT0_S) | total_len;
        USB0.in_ep_reg[epnum].diepctl |= USB_D_EPENA1_M | USB_D_CNAK1_M; // Enable | CNAK

        // For ISO endpoint with interval=1 set correct DATA0/DATA1 bit for next frame
        if ((USB0.in_ep_reg[epnum].diepctl & USB_D_EPTYPE0_M) == (1 << USB_D_EPTYPE1_S) && xfer->interval == 1) {
            // Take odd/even bit from frame counter.
            uint32_t const odd_frame_now = (USB0.dsts & (1u << USB_SOFFN_S));
            USB0.in_ep_reg[epnum].diepctl |= (odd_frame_now ? USB_DI_SETD0PID1 : USB_DI_SETD1PID1);
        }

        // Enable fifo empty interrupt only if there are something to put in the fifo.
        if(total_len != 0) {
            USB0.dtknqr4_fifoemptymsk |= (1 << epnum);
        }
    } else {
        // Each complete packet for OUT xfers triggers XFRC.
        USB0.out_ep_reg[epnum].doeptsiz |= USB_PKTCNT0_M | ((xfer->max_size & USB_XFERSIZE0_V) << USB_XFERSIZE0_S);
        USB0.out_ep_reg[epnum].doepctl  |= USB_EPENA0_M | USB_CNAK0_M;

        // For ISO endpoint with interval=1 set correct DATA0/DATA1 bit for next frame
        if ((USB0.out_ep_reg[epnum].doepctl & USB_D_EPTYPE0_M) == (1 << USB_D_EPTYPE1_S) && xfer->interval == 1) {
            // Take odd/even bit from frame counter.
            uint32_t const odd_frame_now = (USB0.dsts & (1u << USB_SOFFN_S));
            USB0.out_ep_reg[epnum].doepctl |= (odd_frame_now ? USB_DO_SETD0PID1 : USB_DO_SETD1PID1);
        }
    }
}

void usb_device_start_transfer(uint8_t ep_address, uint8_t *buf, uint16_t total_len)
{
    uint8_t const epnum = ep_get_num(ep_address);
    uint8_t const dir   = ep_get_dir(ep_address);

    xfer_ctl_t *xfer = XFER_CTL_BASE(epnum, dir);
    
    // UDPIH edit: special control handling for handling more than 64 bytes in ep0 fifo
    if (ep_address == EP0_IN_ADDR) {
        // The control endpoint can only queue 64 bytes in the FIFO,
        // make sure the entire transfer still goes through
        control_xfer_status.buffer = buf;
        control_xfer_status.total_len = total_len;
        control_xfer_status.transferred_len = 0;

        // Size of initial transfer
        uint16_t to_transfer = (total_len < xfer->max_size) ? total_len : xfer->max_size;

        // Start the transfer
        usb_device_xfer(ep_address, buf, to_transfer);
        return;
    }

    usb_device_xfer(ep_address, buf, total_len);
}

void usb_device_stall_endpoint(uint8_t ep_address)
{
    usb_out_endpoint_t *out_ep = &(USB0.out_ep_reg[0]);
    usb_in_endpoint_t *in_ep = &(USB0.in_ep_reg[0]);

    uint8_t const epnum = ep_get_num(ep_address);
    uint8_t const dir = ep_get_dir(ep_address);

    if (dir == TUSB_DIR_IN) {
        // Only disable currently enabled non-control endpoint
        if ((epnum == 0) || !(in_ep[epnum].diepctl & USB_D_EPENA1_M)) {
            in_ep[epnum].diepctl |= (USB_DI_SNAK1_M | USB_D_STALL1_M);
        } else {
            // Stop transmitting packets and NAK IN xfers.
            in_ep[epnum].diepctl |= USB_DI_SNAK1_M;
            while ((in_ep[epnum].diepint & USB_DI_SNAK1_M) == 0) ;

            // Disable the endpoint. Note that both SNAK and STALL are set here.
            in_ep[epnum].diepctl |= (USB_DI_SNAK1_M | USB_D_STALL1_M | USB_D_EPDIS1_M);
            while ((in_ep[epnum].diepint & USB_D_EPDISBLD0_M) == 0) ;
            in_ep[epnum].diepint = USB_D_EPDISBLD0_M;
        }

        // Flush the FIFO, and wait until we have confirmed it cleared.
        uint8_t const fifo_num = ((in_ep[epnum].diepctl >> USB_D_TXFNUM1_S) & USB_D_TXFNUM1_V);
        USB0.grstctl |= (fifo_num << USB_TXFNUM_S);
        USB0.grstctl |= USB_TXFFLSH_M;
        while ((USB0.grstctl & USB_TXFFLSH_M) != 0) ;
    } else {
        // Only disable currently enabled non-control endpoint
        if ((epnum == 0) || !(out_ep[epnum].doepctl & USB_EPENA0_M)) {
            out_ep[epnum].doepctl |= USB_STALL0_M;
        } else {
            // Asserting GONAK is required to STALL an OUT endpoint.
            // Simpler to use polling here, we don't use the "B"OUTNAKEFF interrupt
            // anyway, and it can't be cleared by user code. If this while loop never
            // finishes, we have bigger problems than just the stack.
            USB0.dctl |= USB_SGOUTNAK_M;
            while ((USB0.gintsts & USB_GOUTNAKEFF_M) == 0) ;

            // Ditto here- disable the endpoint. Note that only STALL and not SNAK
            // is set here.
            out_ep[epnum].doepctl |= (USB_STALL0_M | USB_EPDIS0_M);
            while ((out_ep[epnum].doepint & USB_EPDISBLD0_M) == 0) ;
            out_ep[epnum].doepint = USB_EPDISBLD0_M;

            // Allow other OUT endpoints to keep receiving.
            USB0.dctl |= USB_CGOUTNAK_M;
        }
    }
}

static void receive_packet(xfer_ctl_t *xfer, /* usb_out_endpoint_t * out_ep, */ uint16_t xfer_size)
{
    ESP_EARLY_LOGV(TAG, "USB - receive_packet");
    volatile uint32_t *rx_fifo = USB0.fifo[0];

    uint16_t remaining = xfer->total_len - xfer->queued_len;
    uint16_t to_recv_size;

    if (remaining <= xfer->max_size) {
        // Avoid buffer overflow.
        to_recv_size = (xfer_size > remaining) ? remaining : xfer_size;
    } else {
        // Room for full packet, choose recv_size based on what the microcontroller
        // claims.
        to_recv_size = (xfer_size > xfer->max_size) ? xfer->max_size : xfer_size;
    }

    uint8_t to_recv_rem = to_recv_size % 4;
    uint16_t to_recv_size_aligned = to_recv_size - to_recv_rem;

    // Do not assume xfer buffer is aligned.
    uint8_t *base = (xfer->buffer + xfer->queued_len);

    // This for loop always runs at least once- skip if less than 4 bytes
    // to collect.
    if (to_recv_size >= 4) {
        for (uint16_t i = 0; i < to_recv_size_aligned; i += 4) {
            uint32_t tmp = (*rx_fifo);
            base[i] = tmp & 0x000000FF;
            base[i + 1] = (tmp & 0x0000FF00) >> 8;
            base[i + 2] = (tmp & 0x00FF0000) >> 16;
            base[i + 3] = (tmp & 0xFF000000) >> 24;
        }
    }

    // Do not read invalid bytes from RX FIFO.
    if (to_recv_rem != 0) {
        uint32_t tmp = (*rx_fifo);
        uint8_t *last_32b_bound = base + to_recv_size_aligned;

        last_32b_bound[0] = tmp & 0x000000FF;
        if (to_recv_rem > 1) {
            last_32b_bound[1] = (tmp & 0x0000FF00) >> 8;
        }
        if (to_recv_rem > 2) {
            last_32b_bound[2] = (tmp & 0x00FF0000) >> 16;
        }
    }

    xfer->queued_len += xfer_size;

    // Per USB spec, a short OUT packet (including length 0) is always
    // indicative of the end of a transfer (at least for ctl, bulk, int).
    xfer->short_packet = (xfer_size < xfer->max_size);
}

static void transmit_packet(xfer_ctl_t *xfer, volatile usb_in_endpoint_t *in_ep, uint8_t fifo_num)
{
    ESP_EARLY_LOGV(TAG, "USB - transmit_packet");
    volatile uint32_t *tx_fifo = USB0.fifo[fifo_num];

    uint16_t remaining = (in_ep->dieptsiz & 0x7FFFFU) >> USB_D_XFERSIZE0_S;
    xfer->queued_len = xfer->total_len - remaining;

    uint16_t to_xfer_size = (remaining > xfer->max_size) ? xfer->max_size : remaining;

    uint8_t to_xfer_rem = to_xfer_size % 4;
    uint16_t to_xfer_size_aligned = to_xfer_size - to_xfer_rem;

    ESP_EARLY_LOGV(TAG, "USB - transmit_packet - remaining %u to_xfer_size %u", remaining, to_xfer_size);

    // Buffer might not be aligned to 32b, so we need to force alignment
    // by copying to a temp var.
    uint8_t *base = (xfer->buffer + xfer->queued_len);

    // This for loop always runs at least once- skip if less than 4 bytes
    // to send off.
    if (to_xfer_size >= 4) {
        for (uint16_t i = 0; i < to_xfer_size_aligned; i += 4) {
            uint32_t tmp = base[i] | (base[i + 1] << 8) |
                (base[i + 2] << 16) | (base[i + 3] << 24);
            (*tx_fifo) = tmp;
        }
    }

    // Do not read beyond end of buffer if not divisible by 4.
    if (to_xfer_rem != 0) {
        uint32_t tmp = 0;
        uint8_t *last_32b_bound = base + to_xfer_size_aligned;

        tmp |= last_32b_bound[0];
        if (to_xfer_rem > 1) {
            tmp |= (last_32b_bound[1] << 8);
        }
        if (to_xfer_rem > 2) {
            tmp |= (last_32b_bound[2] << 16);
        }

        (*tx_fifo) = tmp;
    }
}

static void read_rx_fifo(void)
{
    // Pop control word off FIFO (completed xfers will have 2 control words,
    // we only pop one ctl word each interrupt).
    uint32_t const ctl_word = USB0.grxstsp;
    uint8_t  const pktsts   = (ctl_word & USB_PKTSTS_M) >> USB_PKTSTS_S;
    uint8_t  const epnum    = (ctl_word & USB_CHNUM_M ) >> USB_CHNUM_S;
    uint16_t const bcnt     = (ctl_word & USB_BCNT_M  ) >> USB_BCNT_S;

    switch (pktsts) {
        case 0x01: // Global OUT NAK (Interrupt)
            ESP_EARLY_LOGV(TAG, "TUSB IRQ - RX type : Global OUT NAK");
            break;

        case 0x02: { // Out packet recvd
            ESP_EARLY_LOGV(TAG, "TUSB IRQ - RX type : Out packet");
            xfer_ctl_t *xfer = XFER_CTL_BASE(epnum, TUSB_DIR_OUT);
            receive_packet(xfer, bcnt);
        }
        break;

        case 0x03: // Out packet done (Interrupt)
            ESP_EARLY_LOGV(TAG, "TUSB IRQ - RX type : Out packet done");
            break;

        case 0x04: // Step 2: Setup transaction completed (Interrupt)
            // After this event, OEPINT interrupt will occur with SETUP bit set
            ESP_EARLY_LOGV(TAG, "TUSB IRQ - RX : Setup packet done");
            USB0.out_ep_reg[epnum].doeptsiz |= USB_SUPCNT0_M;
            break;

        case 0x06: { // Step1: Setup data packet received
            volatile uint32_t *rx_fifo = USB0.fifo[0];

            // We can receive up to three setup packets in succession, but
            // only the last one is valid. Therefore we just overwrite it
            _setup_packet[0] = (*rx_fifo);
            _setup_packet[1] = (*rx_fifo);

            ESP_EARLY_LOGV(TAG, "TUSB IRQ - RX : Setup packet : 0x%08x 0x%08x", _setup_packet[0], _setup_packet[1]);
        }
        break;

        default: // Invalid, do something here, like breakpoint?
            break;
    }
}

static void handle_epout_ints(void)
{
    // GINTSTS will be cleared with DAINT == 0
    // DAINT for a given EP clears when DOEPINTx is cleared.
    // DOEPINT will be cleared when DAINT's out bits are cleared.
    for (int n = 0; n < USB_OUT_EP_NUM; n++) {
        xfer_ctl_t *xfer = XFER_CTL_BASE(n, TUSB_DIR_OUT);

        if (USB0.daint & (1 << (16 + n))) {
            // SETUP packet Setup Phase done.
            if ((USB0.out_ep_reg[n].doepint & USB_SETUP0_M)) {
                USB0.out_ep_reg[n].doepint = USB_STUPPKTRCVD0_M | USB_SETUP0_M; // clear
                usb_setup_packet_callback((struct usb_ctrlrequest *)&_setup_packet[0]);
            }

            // OUT XFER complete (single packet).q
            if (USB0.out_ep_reg[n].doepint & USB_XFERCOMPL0_M) {

                ESP_EARLY_LOGV(TAG, "TUSB IRQ - EP OUT - XFER complete (single packet)");
                USB0.out_ep_reg[n].doepint = USB_XFERCOMPL0_M;

                // Transfer complete if short packet or total len is transferred
                if (xfer->short_packet || (xfer->queued_len == xfer->total_len)) {
                    xfer->short_packet = false;
                    usb_xfer_complete_callback(n, xfer->queued_len);
                } else {
                    // Schedule another packet to be received.
                    USB0.out_ep_reg[n].doeptsiz |= USB_PKTCNT0_M | ((xfer->max_size & USB_XFERSIZE0_V) << USB_XFERSIZE0_S);
                    USB0.out_ep_reg[n].doepctl |= USB_EPENA0_M | USB_CNAK0_M;
                }
            }
        }
    }
}

static void handle_epin_ints(void)
{
    // GINTSTS will be cleared with DAINT == 0
    // DAINT for a given EP clears when DIEPINTx is cleared.
    // IEPINT will be cleared when DAINT's out bits are cleared.
    for (uint32_t n = 0; n < USB_IN_EP_NUM; n++) {
        xfer_ctl_t *xfer = &xfer_status[n][TUSB_DIR_IN];

        if (USB0.daint & (1 << (0 + n))) {
            ESP_EARLY_LOGV(TAG, "TUSB IRQ - EP IN %u", n);
            // IN XFER complete (entire xfer).
            if (USB0.in_ep_reg[n].diepint & USB_D_XFERCOMPL0_M) {
                ESP_EARLY_LOGV(TAG, "TUSB IRQ - IN XFER complete!");
                USB0.in_ep_reg[n].diepint = USB_D_XFERCOMPL0_M;

                // UDPIH edit: special handling for the control endpoint
                if (n == 0) {
                    control_xfer_status.transferred_len += xfer->total_len;

                    // Check if more packets need to be queued
                    if (control_xfer_status.transferred_len != control_xfer_status.total_len) {
                        // Size of the next transfer
                        uint16_t remaining = control_xfer_status.total_len - control_xfer_status.transferred_len;
                        uint16_t to_transfer = (remaining < xfer->max_size) ? remaining : xfer->max_size;

                        usb_device_xfer(n | 0x80, control_xfer_status.buffer + control_xfer_status.transferred_len, to_transfer);
                    } else {
                        // Call the callback if the total transfer is complete
                        usb_xfer_complete_callback(n | 0x80, control_xfer_status.total_len);
                    }
                } else {
                    usb_xfer_complete_callback(n | 0x80, xfer->total_len);
                }
            }

            // XFER FIFO empty
            if (USB0.in_ep_reg[n].diepint & USB_D_TXFEMP0_M) {
                ESP_EARLY_LOGV(TAG, "TUSB IRQ - IN XFER FIFO empty!");
                USB0.in_ep_reg[n].diepint = USB_D_TXFEMP0_M;
                transmit_packet(xfer, &USB0.in_ep_reg[n], n);

                // Turn off TXFE if all bytes are written.
                if (xfer->queued_len == xfer->total_len) {
                    USB0.dtknqr4_fifoemptymsk &= ~(1 << n);
                }
            }

            // XFER Timeout
            if (USB0.in_ep_reg[n].diepint & USB_D_TIMEOUT0_M) {
                // Clear interrupt or endpoint will hang.
                USB0.in_ep_reg[n].diepint = USB_D_TIMEOUT0_M;
                // Maybe retry?
            }
        }
    }
}

static void _dcd_int_handler(void *arg)
{
    (void) arg;

    const uint32_t int_msk = USB0.gintmsk;
    const uint32_t int_status = USB0.gintsts & int_msk;

    ESP_EARLY_LOGV(TAG, "_int_handler - %x %x", int_msk, int_status);

    if (int_status & USB_USBRST_M) {
        // start of reset
        ESP_EARLY_LOGV(TAG, "dcd_int_handler - reset");
        USB0.gintsts = USB_USBRST_M;
        bus_reset();
    }

    if (int_status & USB_RESETDET_M) {
        ESP_EARLY_LOGV(TAG, "dcd_int_handler - reset while suspend");
        USB0.gintsts = USB_RESETDET_M;
        bus_reset();
    }

    if (int_status & USB_ENUMDONE_M) {
        // ENUMDNE detects speed of the link. For full-speed, we
        // always expect the same value. This interrupt is considered
        // the end of reset.
        USB0.gintsts = USB_ENUMDONE_M;
        enum_done_processing();
    }

    if (int_status & USB_USBSUSP_M) {
        USB0.gintsts = USB_USBSUSP_M;

        ESP_EARLY_LOGV(TAG, "dcd_int_handler - USB_USBSUSP_M");
    }

    if (int_status & USB_WKUPINT_M) {
        USB0.gintsts = USB_WKUPINT_M;

        ESP_EARLY_LOGV(TAG, "dcd_int_handler - USB_USBSUSP_M");
    }

    if (int_status & USB_OTGINT_M) {
        // OTG INT bit is read-only
        ESP_EARLY_LOGV(TAG, "dcd_int_handler - USB_OTGINT_M");

        uint32_t const otg_int = USB0.gotgint;

        if (otg_int & USB_SESENDDET_M) {
            ESP_EARLY_LOGV(TAG, "dcd_int_handler - USB_SESENDDET_M");
        }

        USB0.gotgint = otg_int;
    }

    if (int_status & USB_SOF_M) {
        USB0.gintsts = USB_SOF_M;

        // Disable SOF interrupt since currently only used for remote wakeup detection
        USB0.gintmsk &= ~USB_SOFMSK_M;

        ESP_EARLY_LOGV(TAG, "dcd_int_handler - USB_SOF_M");
    }


    if (int_status & USB_RXFLVI_M) {
        // RXFLVL bit is read-only
        ESP_EARLY_LOGV(TAG, "dcd_int_handler - rx!");

        // Mask out RXFLVL while reading data from FIFO
        USB0.gintmsk &= ~USB_RXFLVIMSK_M;
        read_rx_fifo();
        USB0.gintmsk |= USB_RXFLVIMSK_M;
    }

    // OUT endpoint interrupt handling.
    if (int_status & USB_OEPINT_M) {
        // OEPINT is read-only
        ESP_EARLY_LOGV(TAG, "dcd_int_handler - OUT endpoint!");
        handle_epout_ints();
    }

    // IN endpoint interrupt handling.
    if (int_status & USB_IEPINT_M) {
        // IEPINT bit read-only
        ESP_EARLY_LOGV(TAG, "dcd_int_handler - IN endpoint!");
        handle_epin_ints();
    }

    // Without handling
    USB0.gintsts |= USB_CURMOD_INT_M  |
                    USB_MODEMIS_M     |
                    USB_OTGINT_M      |
                    USB_NPTXFEMP_M    |
                    USB_GINNAKEFF_M   |
                    USB_GOUTNAKEFF    |
                    USB_ERLYSUSP_M    |
                    USB_USBSUSP_M     |
                    USB_ISOOUTDROP_M  |
                    USB_EOPF_M        |
                    USB_EPMIS_M       |
                    USB_INCOMPISOIN_M |
                    USB_INCOMPIP_M    |
                    USB_FETSUSP_M     |
                    USB_PTXFEMP_M;
}
