#pragma once

#include <stdint.h>

typedef struct {
    uint8_t type;
    uint8_t flags;
    uint16_t len;
    uint16_t reserved;
    uint16_t cmd;
} __attribute__((packed)) wmi_mbox_cmd_send_header_t;

typedef struct {
    uint8_t type;
    uint8_t flags;
    uint16_t len;
    uint16_t reserved;
    uint16_t unknown;
    uint8_t dest_mac[6];
    uint8_t source_mac[6];
    uint16_t network_len;
} __attribute__((packed)) wmi_mbox_data_send_header_t;

#define MBOX_SEND_TYPE_WMI 1
#define MBOX_SEND_TYPE_DATA 2

#define MBOX_SEND_FLAGS_REQUEST_ACK 1

typedef struct {
    uint8_t type;
    uint8_t flags;
    uint16_t len;
    uint8_t ack_len;
    uint8_t unknown;
} __attribute__((packed)) wmi_mbox_recv_header_t;

typedef struct {
    uint8_t type;
    uint8_t flags;
    uint16_t len;
    uint8_t ack_len;
    uint8_t unknown;
    uint8_t rssi;
    uint8_t unknown2;
    uint8_t destination_mac[6];
    uint8_t source_mac[6];
    uint16_t network_len;
    uint8_t body[];
} __attribute__((packed)) wmi_mbox_recv_data_header_t;

#define MBOX_RECV_TYPE_ACK_ONLY 0
#define MBOX_RECV_TYPE_WMI_EVENT 1
#define MBOX_RECV_TYPE_DATA_PACKET 2

#define MBOX_RECV_FLAGS_HAS_ACK 2

#define PROTOCOL_ETHER_EAPOL 0x8e88 // TODO: Move this somewhere else

#define WMI_CONNECT_CMD 0x1
#define WMI_SYNCHRONIZE_CMD 0x4
#define WMI_CREATE_PSTREAM_CMD 0x5
#define WMI_START_SCAN_CMD 0x7
#define WMI_SET_SCAN_PARAMS_CMD 0x8
#define WMI_SET_BSS_FILTER_CMD 0x9
#define WMI_SET_PROBED_SSID_CMD 0xA
#define WMI_SET_DISCONNECT_TIMEOUT_CMD 0xD
#define WMI_GET_CHANNEL_LIST_CMD 0xE
#define WMI_SET_CHANNEL_PARAMS_CMD 0x11
#define WMI_SET_POWER_MODE_CMD 0x12
#define WMI_ADD_CIPHER_KEY_CMD 0x16
#define WMI_TARGET_ERROR_REPORT_BITMASK_CMD 0x22
#define WMI_EXTENSION_CMD 0x2E // Prefix for WMIX cmds
#define WMI_SET_KEEPALIVE_CMD 0x3D
#define WMI_SET_WSC_STATUS_CMD 0x41
#define WMI_START_WHATEVER_TIMER_CMD 0x47 // Seems to be Nintendo specific
#define WMI_SET_FRAMERATES_CMD 0x48
#define WMI_SET_BITRATE_CMD 0xF000

#define WMIX_HB_CHALLENGE_RESP_CMD 0x00002008

#define WMI_GET_CHANNEL_LIST_EVENT 0xE
#define WMI_READY_EVENT 0x1001
#define WMI_CONNECT_EVENT 0x1002
#define WMI_DISCONNECT_EVENT 0x1003
#define WMI_BSSINFO_EVENT 0x1004
#define WMI_REGDOMAIN_EVENT 0x1006
#define WMI_NEIGHBOR_REPORT_EVENT 0x1008
#define WMI_SCAN_COMPLETE_EVENT 0x100A
#define WMI_EXTENSION_EVENT 0x1010

#define WMIX_HB_CHALLENGE_RESP_EVENT 0x3007
#define WMIX_DBGLOG_EVENT 0x3008

#define SCAN_TYPE_LONG 0x0
#define SCAN_TYPE_SHORT 0x1

#define BSS_FILTER_NONE 0x0
#define BSS_FILTER_ALL 0x1
#define BSS_FILTER_PROFILE 0x2
#define BSS_FILTER_ALL_BUT_PROFILE 0x3
#define BSS_FILTER_CURRENT_BSS 0x4
#define BSS_FILTER_ALL_BUT_CURRENT_BSS 0x5
#define BSS_FILTER_PROBED_SSID 0x6

#define SSID_PROBE_FLAG_DISABLE 0x0
#define SSID_PROBE_FLAG_SPECIFIC 0x1
#define SSID_PROBE_FLAG_ANY 0x2

#define SCAN_FLAGS_CONNECT (1 << 0)
#define SCAN_FLAGS_CONNECTED (1 << 1)
#define SCAN_FLAGS_ACTIVE (1 << 2)
#define SCAN_FLAGS_ROAM (1 << 3)
#define SCAN_FLAGS_BSSINFO (1 << 4)
#define SCAN_FLAGS_ENABLE_AUTO (1 << 5)
#define SCAN_FLAGS_ENABLE_ABORT (1 << 6)

#define PHY_MODE_11A 0x1
#define PHY_MODE_11G 0x2
#define PHY_MODE_11AG 0x3
#define PHY_MODE_11B 0x4
#define PHY_MODE_11G_ONLY 0x5

#define NETWORK_INFRA 0x1
#define NETWORK_ADHOC 0x2
#define NETWORK_ADHOC_CREATOR 0x4
#define NETWORK_AP 0x10

#define AUTH_OPEN 0x1 // Open/WPA/WPA2
#define AUTH_SHARED 0x2 // WEP
#define AUTH_LEAP 0x4

#define WMI_NONE_AUTH 0x1 // Open/WEP
#define WMI_WPA_PSK_AUTH 0x3
#define WMI_WPA2_PSK_AUTH 0x5

#define CRYPT_NONE 0x1
#define CRYPT_WEP 0x2
#define CRYPT_TKIP 0x3
#define CRYPT_AES 0x4

#define KEY_USAGE_PAIRWISE 0x0
#define KEY_USAGE_GROUP 0x1
#define KEY_USAGE_TX 0x2

#define KEY_OP_INIT_TSC 0x1
#define KEY_OP_INIT_RSC 0x2
#define KEY_OP_INIT_WAPIPN 0x10



typedef struct {
    wmi_mbox_cmd_send_header_t header;
    uint8_t network_type;
    uint8_t dot11_auth_mode;
    uint8_t auth_mode;
    uint8_t pairwise_crypto_type;
    uint8_t pairwise_cypto_len;
    uint8_t group_crypto_type;
    uint8_t group_crypto_len;
    uint8_t ssid_length;
    uint8_t ssid[32];
    uint16_t channel;
    uint8_t bssid[6];
    uint32_t control_flags;
} __attribute__((packed)) wmi_connect_cmd_t;

typedef struct {
    wmi_mbox_cmd_send_header_t header;
    uint8_t data_sync_map;
} __attribute__((packed)) wmi_synchronize_cmd_t;

typedef struct {
    wmi_mbox_cmd_send_header_t header;
    uint32_t min_service_int;
    uint32_t max_service_int;
    uint32_t inactivity_int;
    uint32_t suspension_time;
    uint32_t service_start_time;
    uint32_t min_data_rate;
    uint32_t mean_data_rate;
    uint32_t peak_data_rate;
    uint32_t max_burst_size;
    uint32_t delay_bound;
    uint32_t min_phy_rate;
    uint32_t sba;
    uint32_t medium_time;
    uint16_t nominal_msdu;
    uint16_t max_msdu;
    uint8_t traffic_class;
    uint8_t traffic_direction;
    uint8_t rx_queue_num;
    uint8_t traffic_type;
    uint8_t voice_ps_capability;
    uint8_t tsid;
    uint8_t user_priority;
} __attribute__((packed)) wmi_create_pstream_cmd_t;

typedef struct {
    wmi_mbox_cmd_send_header_t header;
    uint32_t force_fg_scan;
    uint32_t is_legacy;
    uint32_t home_dwell_time;
    uint32_t force_scan_time;
    uint8_t scan_type;
    uint8_t n_channels;
    uint16_t channels[1];
} __attribute__((packed)) wmi_start_scan_cmd_t;

typedef struct {
    wmi_mbox_cmd_send_header_t header;
    uint16_t fg_start_period;
    uint16_t fg_end_period;
    uint16_t bg_period;
    uint16_t maxact_chdwell_time;
    uint16_t pas_chdwell_time;
    uint8_t short_scan_ratio;
    uint8_t scan_control_flags;
    uint16_t minact_chdwell_time;
    uint16_t maxact_scan_per_ssid;
    uint32_t max_dfsch_act_time;
} __attribute__((packed)) wmi_set_scan_params_cmd_t;

typedef struct {
    wmi_mbox_cmd_send_header_t header;
    uint32_t cmd;
    uint32_t cookie;
    uint32_t source; 
} __attribute__((packed)) wmix_hb_challenge_resp_cmd_t;

typedef struct {
    wmi_mbox_cmd_send_header_t header;
    uint8_t bss_filter;
    uint8_t reserved1;
    uint16_t reserved2;
    uint32_t ie_mask;
} __attribute__((packed)) wmi_set_bss_filter_cmd_t;

typedef struct {
    wmi_mbox_cmd_send_header_t header;
    uint8_t index;
    uint8_t flag;
    uint8_t ssid_length;
    char ssid[32];
} __attribute__((packed)) wmi_set_probed_ssid_cmd_t;

typedef struct {
    wmi_mbox_cmd_send_header_t header;
    uint8_t disconnect_timeout;
} __attribute__((packed)) wmi_set_disconnect_timeout_cmd_t;

typedef struct {
    wmi_mbox_cmd_send_header_t header;
    uint8_t reserved;
    uint8_t scan_param;
    uint8_t phy_mode;
    uint8_t num_channels;
    uint16_t channels[32];
} __attribute__((packed)) wmi_set_channel_params_cmd_t;

typedef struct {
    wmi_mbox_cmd_send_header_t header;
    uint8_t power_mode;
} __attribute__((packed)) wmi_set_power_mode_cmd_t;

typedef struct {
    wmi_mbox_cmd_send_header_t header;
    uint8_t key_index;
    uint8_t key_type;
    uint8_t key_usage;
    uint8_t key_length;
    uint8_t key_rsc[8];
    uint8_t key[32];
    uint8_t key_op_control;
} __attribute__((packed)) wmi_add_cipher_key_cmd_t;

typedef struct {
    wmi_mbox_cmd_send_header_t header;
    uint32_t bitmask;
} __attribute__((packed)) wmi_error_report_cmd_t;

typedef struct {
    wmi_mbox_cmd_send_header_t header;
    uint8_t keepalive_interval;
} __attribute__((packed)) wmi_set_keepalive_cmd_t;

typedef struct {
    wmi_mbox_cmd_send_header_t header;
    uint8_t undocumented;
} __attribute__((packed)) wmi_set_wsc_status_cmd_t;

typedef struct {
    wmi_mbox_cmd_send_header_t header;
    uint32_t time;
} __attribute__((packed)) wmi_start_whatever_timer_cmd_t;

typedef struct {
    wmi_mbox_cmd_send_header_t header;
    uint8_t enable_mask;
    uint8_t frame_type;
    uint32_t frame_rate_mask;
} __attribute__((packed)) wmi_set_framerates_cmd_t;

typedef struct {
    wmi_mbox_cmd_send_header_t header;
    uint8_t rate_index;
    uint8_t management_rate_index;
    uint8_t control_rate_index;
} __attribute__((packed)) wmi_set_bitrate_cmd_t;

typedef struct {
    uint8_t reserved;
    uint8_t n_channels;
    uint16_t channel_list[];
} __attribute__((packed)) wmi_get_channel_list_reply_t;

typedef struct {
    uint16_t channel;
    uint8_t frame_type;
    uint8_t snr;
    uint16_t rssi;
    uint8_t bssid[6];
    uint32_t ie_mask;
    uint8_t body[];
} __attribute__((packed)) wmi_bssinfo_event_t;

typedef struct {
    uint32_t status;
} __attribute__((packed)) wmi_scan_complete_event_t;


void sdio_send_wmi_cmd(uint8_t mbox, wmi_mbox_cmd_send_header_t* xfer_buf);
void sdio_send_wmi_cmd_without_poll(uint8_t mbox, wmi_mbox_cmd_send_header_t* xfer_buf);

void sdio_heartbeat(uint8_t mbox);
void sdio_tx_callback(void);

void sdio_poll_mbox(uint8_t mbox);

void sdio_wmi_connect_cmd(uint8_t mbox, wmi_connect_cmd_t* cmd);
void sdio_wmi_synchronize_cmd(uint8_t mbox, uint16_t unknown, uint8_t data_sync_map);
void sdio_wmi_create_pstream_cmd(uint8_t mbox, wmi_create_pstream_cmd_t* cmd);
void sdio_wmi_start_scan_cmd(uint8_t mbox, uint8_t type);
void sdio_wmi_set_scan_params_cmd(uint8_t mbox, wmi_set_scan_params_cmd_t* cmd);
void sdio_wmi_set_bss_filter_cmd(uint8_t mbox, uint8_t bss_filter, uint32_t ie_mask);
void sdio_wmi_set_probed_ssid_cmd(uint8_t mbox, uint8_t flag, char* ssid);
void sdio_wmi_set_disconnect_timeout_cmd(uint8_t mbox, uint8_t timeout);
void sdio_wmi_set_channel_params_cmd(uint8_t mbox, uint8_t scan_param, uint8_t phy_mode, uint8_t n_channels, uint16_t* channels);
void sdio_wmi_set_power_mode_cmd(uint8_t mbox, uint8_t power_mode);
void sdio_wmi_add_cipher_key_cmd(uint8_t mbox, uint8_t index, uint8_t type, uint8_t usage, uint8_t op, uint8_t key_length, uint8_t* key);
void sdio_wmi_get_channel_list_cmd(uint8_t mbox);
void sdio_wmi_error_report_cmd(uint8_t mbox, uint32_t bitmask);
void sdio_wmi_set_keepalive_cmd(uint8_t mbox, uint8_t interval);
void sdio_wmi_set_wsc_status_cmd(uint8_t mbox, uint8_t undocumented);
void sdio_wmi_start_whatever_timer_cmd(uint8_t mbox, uint32_t time);
void sdio_wmi_set_framerates_cmd(uint8_t mbox, uint8_t enable_mask, uint8_t frame_type, uint32_t frame_type_mask);
void sdio_wmi_set_bitrate_cmd(uint8_t mbox, uint8_t rate_index, uint8_t management_rate, uint8_t control_rate);

void sdio_wmi_scan_channel(void);
void sdio_wmi_connect(void);

void sdio_send_wmi_data(wmi_mbox_data_send_header_t* packet);