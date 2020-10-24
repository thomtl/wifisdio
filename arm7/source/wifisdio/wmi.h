#pragma once

#include <stdint.h>

typedef struct {
    uint8_t type;
    uint8_t flags;
    uint16_t len;
    uint16_t reserved;
    uint16_t cmd;
} __attribute__((packed)) wmi_mbox_send_header_t;

#define MBOX_SEND_TYPE_WMI 1
#define MBOX_SEND_FLAGS_REQUEST_ACK 1

typedef struct {
    uint8_t type;
    uint8_t flags;
    uint16_t len;
    uint8_t ack_len;
    uint8_t unknown;
} __attribute__((packed)) wmi_mbox_recv_header_t;

#define MBOX_RECV_TYPE_ACK_ONLY 0
#define MBOX_RECV_TYPE_WMI_EVENT 1
#define MBOX_RECV_TYPE_DATA_PACKET 2

#define MBOX_RECV_FLAGS_HAS_ACK 2

#define PROTOCOL_ETHER_EAPOL 0x8e88 // TODO: Move this somewhere else

#define WMI_GET_CHANNEL_LIST_CMD 0xE
#define WMI_TARGET_ERROR_REPORT_BITMASK_CMD 0x22
#define WMI_EXTENSION_CMD 0x2E // Prefix for WMIX cmds
#define WMI_START_WHATEVER_TIMER_CMD 0x47 // Seems to be Nintendo specific

#define WMIX_HB_CHALLENGE_RESP_CMD 0x00002008

#define WMI_GET_CHANNEL_LIST_EVENT 0xE
#define WMI_READY_EVENT 0x1001
#define WMI_REGDOMAIN_EVENT 0x1006

typedef struct {
    wmi_mbox_send_header_t header;
    uint32_t cmd;
    uint32_t cookie;
    uint32_t source; 
} __attribute__((packed)) wmix_hb_challenge_resp_cmd_t;

typedef struct {
    wmi_mbox_send_header_t header;
    uint32_t bitmask;
} __attribute__((packed)) wmi_error_report_cmd_t;

typedef struct {
    wmi_mbox_send_header_t header;
    uint32_t time;
} __attribute__((packed)) wmi_start_whatever_timer_cmd_t;

typedef struct {
    uint8_t reserved;
    uint8_t n_channels;
    uint16_t channel_list[];
} __attribute__((packed)) wmi_get_channel_list_reply_t;


void sdio_send_wmi_cmd(uint8_t mbox, wmi_mbox_send_header_t* xfer_buf);
void sdio_send_wmi_cmd_without_poll(uint8_t mbox, wmi_mbox_send_header_t* xfer_buf);

void sdio_heartbeat(uint8_t mbox);
void sdio_tx_callback(void);

void sdio_poll_mbox(uint8_t mbox);

void sdio_wmi_get_channel_list_cmd(uint8_t mbox);
void sdio_wmi_error_report_cmd(uint8_t mbox, uint32_t bitmask);
void sdio_wmi_start_whatever_timer_cmd(uint8_t mbox, uint32_t time);