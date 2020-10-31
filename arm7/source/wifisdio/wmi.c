#include "wifisdio.h"
#include "wmi.h"
#include "wifi.h"

#include "sdio.h"

#include <stdio.h>
#include <string.h>

uint8_t ath_cmd_ack_pending = 0;
uint8_t ath_data_ack_pending = 0;
uint8_t ath_await_scan_complete = 0;
uint8_t ath_await_connect_complete = 0;

uint32_t ath_lookahead_value = 0;
uint8_t ath_lookahead_flag = 0;

uint32_t recent_heartbeat = 0;
uint32_t** curr_wpa_tx_callback_list_ptr = NULL;

void sdio_send_wmi_cmd_without_poll(uint8_t mbox, wmi_mbox_send_header_t* cmd) {
    sdio_send_mbox_block(mbox, (uint8_t*)cmd);
    ath_cmd_ack_pending = (cmd->flags & MBOX_SEND_FLAGS_REQUEST_ACK) ? (1) : (0);
}

void sdio_send_wmi_cmd(uint8_t mbox, wmi_mbox_send_header_t* cmd) {
    do {
        sdio_poll_mbox(mbox);
    } while (ath_cmd_ack_pending != 0);

    sdio_send_mbox_block(mbox, (uint8_t*)cmd);
    ath_cmd_ack_pending = (cmd->flags & MBOX_SEND_FLAGS_REQUEST_ACK) ? (1) : (0);
}


void sdio_heartbeat(uint8_t mbox) {
    extern uint32_t arm7_count_60hz;

    uint32_t ticks = arm7_count_60hz - recent_heartbeat;
    if(ticks < 60)
        return;

    if(ath_cmd_ack_pending != 0) // Exit if old cmd is still busy
        return;

    recent_heartbeat = arm7_count_60hz;

    wmix_hb_challenge_resp_cmd_t cmd = {0};
    cmd.header.type = MBOX_SEND_TYPE_WMI;
    cmd.header.flags = MBOX_SEND_FLAGS_REQUEST_ACK;
    cmd.header.len = sizeof(wmix_hb_challenge_resp_cmd_t) - 6;
    cmd.header.cmd = WMI_EXTENSION_CMD;
    cmd.cmd = WMIX_HB_CHALLENGE_RESP_CMD;

    static uint32_t cookie = 1;
    cmd.cookie = cookie++;

    sdio_send_wmi_cmd_without_poll(mbox, &cmd.header);
}

void sdio_tx_callback(void) {
    uint32_t* item = *curr_wpa_tx_callback_list_ptr;
    if(!item)
        return;

    uint8_t ath = ath_cmd_ack_pending | ath_data_ack_pending;
    if(ath != 0)
        return;

    void (*proc)() = (void(*)())*item;
    *curr_wpa_tx_callback_list_ptr = item + 1;

    if(proc) {
        proc();
        return;
    }

    *curr_wpa_tx_callback_list_ptr = 0;
}

void sdio_Wifi_Intr_TxEnd(void) {
    // TODO
}

void sdio_Wifi_Intr_RxEapolEnd(void) {
    panic("sdio_Wifi_Intr_RxEapolEnd()\n");
}

void sdio_Wifi_Intr_RxEnd(void) {
    panic("sdio_Wifi_Intr_RxEnd()\n");
}

void sdio_poll_mbox(uint8_t mbox) {
    int old_ime = enterCriticalSection();

    const uint8_t max_polls_per_call = 10;
    for(size_t i = 0; i < max_polls_per_call; i++) {
        sdio_heartbeat(mbox);
        //sdio_tx_callback();
        sdio_Wifi_Intr_TxEnd();

        if(ath_lookahead_flag == 0) {
            sdio_check_mbox_state();
            uint8_t host_int_status = ((uint8_t*)sdio_xfer_buf)[0];
            uint8_t mbox_frame = 1;//((uint8_t*)sdio_xfer_buf)[4];
            uint8_t rx_lookahead_valid = ((uint8_t*)sdio_xfer_buf)[5];
            uint32_t rx_lookahead0 = sdio_xfer_buf[2]; // TODO(thom_tl): This is hardcoded for MBOX0, fix that

            if((host_int_status & 1) == 0) {
                leaveCriticalSection(old_ime);
                return;
            }
        
            if((mbox_frame & 1) == 0) {
                leaveCriticalSection(old_ime);
                return;
            }

            if((rx_lookahead_valid & 1) == 0) {
                leaveCriticalSection(old_ime);
                return;
            }

            ath_lookahead_value = rx_lookahead0;
        }

        size_t size = (ath_lookahead_value >> 16) + 6;
        size += 0x7F;
        size &= ~0x7F;

        sdio_cmd53_read(sdio_xfer_buf, (0x18000000 | FUNC1_MBOX_TOP(mbox)) - size, size >> 7);
        ath_lookahead_flag = 0;

        wmi_mbox_recv_header_t* header = (wmi_mbox_recv_header_t*)sdio_xfer_buf;
        if(header->flags & MBOX_RECV_FLAGS_HAS_ACK) {
            if(header->ack_len > header->len) {
                leaveCriticalSection(old_ime);
                panic("sdio_poll_mbox(): header->ack_len (%x) > header->len (%x)\n", header->ack_len, header->len);
            }

            uint8_t* ack_list = (uint8_t*)(((uintptr_t)header) + ((header->len - header->ack_len) + 6));
            size_t ack_len = header->ack_len;

            while(ack_len != 0) {
                if(ack_len < 2) {
                    leaveCriticalSection(old_ime);
                    panic("sdio_poll_mbox(): Remaining ack_list size is smaller than 1 entry\n");
                }
                ack_len -= 2;

                uint8_t item = *ack_list++;
                uint8_t len = *ack_list++;
                if(len > ack_len) {
                    leaveCriticalSection(old_ime);
                    panic("sdio_poll_mbox(): ack_list entry is larger than list\n");
                }
                ack_len -= len;

                if(item == 1) {
                    while(len != 0) {
                        if(len < 2) {
                            leaveCriticalSection(old_ime);
                            panic("sdio_poll_mbox(): ack_list len is smaller than entry header\n");
                        }
                        len -= 2;

                        uint8_t item_type = *ack_list++;
                        if(item_type == 0x1) // CMD Ack
                            ath_cmd_ack_pending = 0; // Clear pend
                        else if(item_type == 0x2 || item_type == 0x5) // Data Ack
                            ath_data_ack_pending = 0; // Clear pend
                    
                        uint8_t item_count = *ack_list++;
                        if(item_count != 1) {
                            leaveCriticalSection(old_ime);
                            panic("sdio_poll_mbox(): Unknown ack_list item count\n");
                        }
                    }
                } else if(item == 2) {
                    if(len != 6) {
                        leaveCriticalSection(old_ime);
                        panic("sdio_poll_mbox(): Unknown ack_list lookahead len\n");
                    }

                    uint8_t id1 = *ack_list++;
                    uint8_t low = *ack_list++;
                    uint8_t midl = *ack_list++;
                    uint8_t midh = *ack_list++;
                    uint8_t high = *ack_list++;
                    uint8_t id2 = *ack_list++;

                    if(id1 == 0x55 && id2 == 0xAA) {
                        uint32_t lookahead = low | (midl << 8) | (midh << 16) | (high << 24);
                        ath_lookahead_value = lookahead;
                        ath_lookahead_flag = 1;
                    }
                } else {
                    leaveCriticalSection(old_ime);
                    panic("sdio_poll_mbox(): Unknown ack_list entry type %d\n", item);
                }
            }
        }
        
        if(header->type == MBOX_RECV_TYPE_ACK_ONLY) {
            if(!(header->flags & MBOX_RECV_FLAGS_HAS_ACK)) {
                leaveCriticalSection(old_ime);
                panic("sdio_poll_mbox(): ACK_ONLY header has no ack\n");
            }
            
            if(header->len != header->ack_len) {
                leaveCriticalSection(old_ime);
                panic("sdio_poll_bmox(): ACK_ONLY header ack len does not match len\n");
            }
        } else if(header->type == MBOX_RECV_TYPE_WMI_EVENT) {
            uint16_t* data = (uint16_t*)sdio_xfer_buf;

            uint16_t event = data[3];
            uint16_t len = header->len;
            uint16_t* params = data + 4;

            uint8_t xtra_len = (header->flags & MBOX_RECV_FLAGS_HAS_ACK) ? (header->ack_len) : (0);
            xtra_len += 2; // Add cmd

            if(len < xtra_len) {
                leaveCriticalSection(old_ime);
                panic("sdio_poll_mbox(): Invalid WMI Event packet len\n");
            }
            len -= xtra_len;
            
            switch(event) {
                case WMI_GET_CHANNEL_LIST_EVENT: {
                    wmi_get_channel_list_reply_t* reply = (wmi_get_channel_list_reply_t*)params;
                    uint32_t channel_mask = 0;
                    for(size_t j = 0; j < reply->n_channels; j++) {
                        uint16_t channel = reply->channel_list[j];

                        channel -= 0x900;
                        if(channel == 0xB4) {
                            channel_mask |= (1 << 14);
                            continue;
                        }

                        channel -= 0x6C;
                        if(channel > (0x9a8 - 0x96c))
                            continue;

                        if((channel % 5) == 0)
                            channel_mask |= (1 << ((channel / 5) + 1));                    
                    }
                    
                    extern uint32_t regulatory_channels;
                    regulatory_channels = channel_mask;

                    print("Channel Mask: 0x%lx\n", channel_mask);
                    break;
                }
                case WMI_READY_EVENT: {
                    uint8_t* mac = (uint8_t*)params;
                    print("MAC %02X:%02X:%02X:%02X:%02X:%02X\n", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

                    break;
                }
                case WMI_CONNECT_EVENT: {
                    print("WMI CONNECT\n");
                    ath_await_connect_complete = 0;
                    break;
                }
                case WMI_BSSINFO_EVENT: {
                    wmi_bssinfo_event_t* info = (wmi_bssinfo_event_t*)params;
                    WifiBeacon_t beacon = {0};

                    beacon.strength = info->snr;
                    memcpy(beacon.sa, info->bssid, 6);
                    memcpy(beacon.bssid, info->bssid, 6);

                    len -= sizeof(WifiBeacon_t);

                    uint32_t flags = sgWifiAp_FLAGS_ACTIVE | sgWifiAp_FLAGS_COMPATIBLE;
                    if(memcmp(beacon.sa, beacon.bssid, 6) != 0)
                        flags |= sgWifiAp_FLAGS_ADHOC;

                    size_t j = 0;
                    if(len < (8 + 2))
                        break;

                    j += 8; // Skip Timestap
                    j += 2; // Skip BeaconInterval

                    if(len < 2)
                        break;
                    
                    uint16_t capability = info->body[j] | (info->body[j + 1]);
                    j += 2; // Skip Capability

                    if(capability & 0x10)
                        flags |= sgWifiAp_FLAGS_WEP;

                    uint8_t ssid_len = 0;
                    char* ssid = NULL;

                    uint8_t* rate_set;
                    extern uint16_t current_channel;
                    uint16_t channel = current_channel;

                    extern WifiAp_t access_points[6];

                    while(j < len) {
                        uint8_t item_type = info->body[j++];
                        uint8_t item_len = info->body[j++];

                        switch(item_type) {
                            case WIFI_MANAGEMENT_ELEMENT_ID_SSID: {
                                //print("SSID: %.*s\n", item_len, &info->body[j]);
                                ssid_len = item_len;
                                ssid = &info->body[j];
                                break;
                            }
                            case WIFI_MANAGEMENT_ELEMENT_ID_RATE_SET: {
                                rate_set = &info->body[j - 1];
                                break;
                            }
                            case WIFI_MANAGEMENT_ELEMENT_ID_DS_SET: {
                                if(item_len > 1)
                                    channel = info->body[j];
                                break;
                            }
                        }

                        j += item_len;
                    }

                    for(size_t i = 0; i < 6; i++) {
                        if((access_points[i].enable & 0x80) == 0)
                            continue;

                        if(strncmp(access_points[i].ssid, ssid, ssid_len) == 0) {
                            memcpy(access_points[i].bssid, beacon.bssid, 6);
                            access_points[i].channel = current_channel;

                            extern bool ap_found;
                            extern uint8_t ap_index;

                            ap_found = true;
                            ap_index = i;
                            break;
                        }
                    }

                    break;
                }
                case WMI_REGDOMAIN_EVENT: {
                    uint8_t* regdomain_ptr = (uint8_t*)params;
                    uint32_t regdomain = regdomain_ptr[0] | (regdomain_ptr[1] << 8) | (regdomain_ptr[2] << 16) | (regdomain_ptr[3] << 24);

                    print("Regulatory Domain: 0x%lx\n", regdomain);
                    
                    extern uint32_t regulatory_domain;
                    regulatory_domain = regdomain;
                    break;
                }
                case WMI_SCAN_COMPLETE_EVENT: {
                    ath_await_scan_complete = 0;
                    wmi_scan_complete_event_t* info = (wmi_scan_complete_event_t*)params;

                    if(len != 4)
                        panic("sdio_poll_mbox(): WMI_SCAN_COMPLETE_EVENT: Unknown len (%x)\n", len);

                    if(info->status != 0 && info->status != 0x10)
                        panic("sdio_poll_mbox(): WMI_SCAN_COMPLETE_EVENT: Unknown scan status (%x)\n", info->status);
                    break;
                }
                case WMI_EXTENSION_EVENT: {
                    uint8_t* wmix_event_ptr = (uint8_t*)params;
                    uint32_t wmix_event = wmix_event_ptr[0] | (wmix_event_ptr[1] << 8) | (wmix_event_ptr[2] << 16) | (wmix_event_ptr[3] << 24);
                    switch(wmix_event) {
                        case WMIX_HB_CHALLENGE_RESP_EVENT:
                        case WMIX_DBGLOG_EVENT:
                            break;
                        default:
                            print("Unknown WMIX Event 0x%x\n", wmix_event);
                    }
                    break;
                }
                default: {
                    //leaveCriticalSection(old_ime);
                    print("Unknown WMI Event 0x%x\n", event);
                }
            }
        } else if(header->type == MBOX_RECV_TYPE_DATA_PACKET || header->type == 5) { // Occurs on 02acc2?
            uint16_t* data = (uint16_t*)sdio_xfer_buf;
            leaveCriticalSection(old_ime); // !! remove this when implementing the funcs below
            if(data[0xE] == PROTOCOL_ETHER_EAPOL)
                sdio_Wifi_Intr_RxEapolEnd();
            else
                sdio_Wifi_Intr_RxEnd();
        } else {
            leaveCriticalSection(old_ime);
            panic("sdio_poll_mbox(): Unknown recv header type\n");
        }
    }

    leaveCriticalSection(old_ime);
}

void sdio_wmi_connect_cmd(uint8_t mbox, wmi_connect_cmd_t* cmd) {
    cmd->header.type = MBOX_SEND_TYPE_WMI;
    cmd->header.flags = MBOX_SEND_FLAGS_REQUEST_ACK;
    cmd->header.len = sizeof(wmi_connect_cmd_t) - 6;
    cmd->header.cmd = WMI_CONNECT_CMD;

    sdio_send_wmi_cmd(mbox, &cmd->header);
}

void sdio_wmi_synchronize_cmd(uint8_t mbox, uint16_t unknown, uint8_t data_sync_map) {
    wmi_synchronize_cmd_t cmd = {0};
    cmd.header.type = MBOX_SEND_TYPE_WMI;
    cmd.header.flags = MBOX_SEND_FLAGS_REQUEST_ACK;
    cmd.header.len = sizeof(wmi_synchronize_cmd_t) - 6;
    cmd.header.reserved = unknown;
    cmd.header.cmd = WMI_SYNCHRONIZE_CMD;

    cmd.data_sync_map = data_sync_map;

    sdio_send_wmi_cmd(mbox, &cmd.header);
}

void sdio_wmi_create_pstream_cmd(uint8_t mbox, wmi_create_pstream_cmd_t* cmd) {
    cmd->header.type = MBOX_SEND_TYPE_WMI;
    cmd->header.flags = MBOX_SEND_FLAGS_REQUEST_ACK;
    cmd->header.len = sizeof(wmi_create_pstream_cmd_t) - 6;
    cmd->header.cmd = WMI_CREATE_PSTREAM_CMD;

    sdio_send_wmi_cmd(mbox, &cmd->header);
}

void sdio_wmi_start_scan_cmd(uint8_t mbox, uint8_t type) {
    wmi_start_scan_cmd_t cmd = {0};
    cmd.header.type = MBOX_SEND_TYPE_WMI;
    cmd.header.flags = MBOX_SEND_FLAGS_REQUEST_ACK;
    cmd.header.len = sizeof(wmi_start_scan_cmd_t) - 6;
    cmd.header.cmd = WMI_START_SCAN_CMD;

    cmd.force_fg_scan = 0;
    cmd.is_legacy = 0;
    cmd.home_dwell_time = 0x14;
    cmd.force_scan_time = 0;
    cmd.scan_type = type;
    cmd.n_channels = 0;
    cmd.channels[0] = 0;

    sdio_send_wmi_cmd(mbox, &cmd.header);
}

void sdio_wmi_set_scan_params_cmd(uint8_t mbox, wmi_set_scan_params_cmd_t* cmd) {
    cmd->header.type = MBOX_SEND_TYPE_WMI;
    cmd->header.flags = MBOX_SEND_FLAGS_REQUEST_ACK;
    cmd->header.len = sizeof(wmi_set_scan_params_cmd_t) - 6;
    cmd->header.cmd = WMI_SET_SCAN_PARAMS_CMD;

    sdio_send_wmi_cmd(mbox, &cmd->header);
}

void sdio_wmi_set_bss_filter_cmd(uint8_t mbox, uint8_t bss_filter, uint32_t ie_mask) {
    wmi_set_bss_filter_cmd_t cmd = {0};
    cmd.header.type = MBOX_SEND_TYPE_WMI;
    cmd.header.flags = MBOX_SEND_FLAGS_REQUEST_ACK;
    cmd.header.len = sizeof(wmi_set_bss_filter_cmd_t) - 6;
    cmd.header.cmd = WMI_SET_BSS_FILTER_CMD;

    cmd.bss_filter = bss_filter;
    cmd.ie_mask = ie_mask;

    sdio_send_wmi_cmd(mbox, &cmd.header);
}

void sdio_wmi_set_probed_ssid_cmd(uint8_t mbox, uint8_t flag, char* ssid) {
    if(strlen(ssid) > 32)
        panic("sdio_wmi_set_probed_ssid_cmd(): Invalid ssid length\n");
    wmi_set_probed_ssid_cmd_t cmd = {0};
    cmd.header.type = MBOX_SEND_TYPE_WMI;
    cmd.header.flags = MBOX_SEND_FLAGS_REQUEST_ACK;
    cmd.header.len = sizeof(wmi_set_probed_ssid_cmd_t) - 6;
    cmd.header.cmd = WMI_SET_PROBED_SSID_CMD;

    cmd.flag = flag;
    strcpy(cmd.ssid, ssid);

    sdio_send_wmi_cmd(mbox, &cmd.header);
}

void sdio_wmi_set_disconnect_timeout_cmd(uint8_t mbox, uint8_t timeout) {
    wmi_set_disconnect_timeout_cmd_t cmd = {0};
    cmd.header.type = MBOX_SEND_TYPE_WMI;
    cmd.header.flags = MBOX_SEND_FLAGS_REQUEST_ACK;
    cmd.header.len = sizeof(wmi_set_disconnect_timeout_cmd_t) - 6;
    cmd.header.cmd = WMI_SET_DISCONNECT_TIMEOUT_CMD;

    cmd.disconnect_timeout = timeout;

    sdio_send_wmi_cmd(mbox, &cmd.header);
}

void sdio_wmi_set_channel_params_cmd(uint8_t mbox, uint8_t scan_param, uint8_t phy_mode, uint8_t n_channels, uint16_t* channels) {
    wmi_set_channel_params_cmd_t cmd = {0};
    cmd.header.type = MBOX_SEND_TYPE_WMI;
    cmd.header.flags = MBOX_SEND_FLAGS_REQUEST_ACK;
    cmd.header.len = sizeof(wmi_set_channel_params_cmd_t) - 6;
    cmd.header.cmd = WMI_SET_CHANNEL_PARAMS_CMD;

    cmd.scan_param = scan_param;
    cmd.phy_mode = phy_mode;
    cmd.num_channels = n_channels;
    for(size_t i = 0; i < n_channels; i++)
        cmd.channels[i] = channels[i];

    sdio_send_wmi_cmd(mbox, &cmd.header);
}

void sdio_wmi_set_power_mode_cmd(uint8_t mbox, uint8_t power_mode) {
    wmi_set_power_mode_cmd_t cmd = {0};
    cmd.header.type = MBOX_SEND_TYPE_WMI;
    cmd.header.flags = MBOX_SEND_FLAGS_REQUEST_ACK;
    cmd.header.len = sizeof(wmi_set_power_mode_cmd_t) - 6;
    cmd.header.cmd = WMI_SET_POWER_MODE_CMD;

    cmd.power_mode = power_mode;

    sdio_send_wmi_cmd(mbox, &cmd.header);
}

void sdio_wmi_get_channel_list_cmd(uint8_t mbox) {
    wmi_mbox_send_header_t cmd = {0};
    cmd.type = MBOX_SEND_TYPE_WMI;
    cmd.flags = MBOX_SEND_FLAGS_REQUEST_ACK;
    cmd.len = sizeof(wmi_mbox_send_header_t) - 6;
    cmd.cmd = WMI_GET_CHANNEL_LIST_CMD;

    sdio_send_wmi_cmd(mbox, &cmd);
}

void sdio_wmi_error_report_cmd(uint8_t mbox, uint32_t bitmask) {
    wmi_error_report_cmd_t cmd = {0};
    cmd.header.type = MBOX_SEND_TYPE_WMI;
    cmd.header.flags = MBOX_SEND_FLAGS_REQUEST_ACK;
    cmd.header.len = sizeof(wmi_error_report_cmd_t) - 6;
    cmd.header.cmd = WMI_TARGET_ERROR_REPORT_BITMASK_CMD;

    cmd.bitmask = bitmask;

    sdio_send_wmi_cmd(mbox, &cmd.header);
}

void sdio_wmi_set_keepalive_cmd(uint8_t mbox, uint8_t interval) {
    wmi_set_keepalive_cmd_t cmd = {0};
    cmd.header.type = MBOX_SEND_TYPE_WMI;
    cmd.header.flags = MBOX_SEND_FLAGS_REQUEST_ACK;
    cmd.header.len = sizeof(wmi_set_keepalive_cmd_t) - 6;
    cmd.header.cmd = WMI_SET_KEEPALIVE_CMD;

    cmd.keepalive_interval = interval;

    sdio_send_wmi_cmd(mbox, &cmd.header);
}

void sdio_wmi_set_wsc_status_cmd(uint8_t mbox, uint8_t undocumented) {
    wmi_set_wsc_status_cmd_t cmd = {0};
    cmd.header.type = MBOX_SEND_TYPE_WMI;
    cmd.header.flags = MBOX_SEND_FLAGS_REQUEST_ACK;
    cmd.header.len = sizeof(wmi_set_wsc_status_cmd_t) - 6;
    cmd.header.cmd = WMI_SET_WSC_STATUS_CMD;

    cmd.undocumented = undocumented;

    sdio_send_wmi_cmd(mbox, &cmd.header);
}

void sdio_wmi_start_whatever_timer_cmd(uint8_t mbox, uint32_t time) {
    wmi_start_whatever_timer_cmd_t cmd = {0};
    cmd.header.type = MBOX_SEND_TYPE_WMI;
    cmd.header.flags = MBOX_SEND_FLAGS_REQUEST_ACK;
    cmd.header.len = sizeof(wmi_start_whatever_timer_cmd_t) - 6;
    cmd.header.cmd = WMI_START_WHATEVER_TIMER_CMD;

    cmd.time = time;

    sdio_send_wmi_cmd(mbox, &cmd.header);
}

void sdio_wmi_set_framerates_cmd(uint8_t mbox, uint8_t enable_mask, uint8_t frame_type, uint32_t frame_type_mask) {
    wmi_set_framerates_cmd_t cmd = {0};
    cmd.header.type = MBOX_SEND_TYPE_WMI;
    cmd.header.flags = MBOX_SEND_FLAGS_REQUEST_ACK;
    cmd.header.len = sizeof(wmi_set_framerates_cmd_t) - 6;
    cmd.header.cmd = WMI_SET_FRAMERATES_CMD;

    cmd.enable_mask = enable_mask;
    cmd.frame_type = frame_type;
    cmd.frame_rate_mask = frame_type_mask;

    sdio_send_wmi_cmd(mbox, &cmd.header);
}

void sdio_wmi_set_bitrate_cmd(uint8_t mbox, uint8_t rate_index, uint8_t management_rate, uint8_t control_rate) {
    wmi_set_bitrate_cmd_t cmd = {0};
    cmd.header.type = MBOX_SEND_TYPE_WMI;
    cmd.header.flags = MBOX_SEND_FLAGS_REQUEST_ACK;
    cmd.header.len = sizeof(wmi_set_bitrate_cmd_t) - 6;
    cmd.header.cmd = WMI_SET_BITRATE_CMD;

    cmd.rate_index = rate_index;
    cmd.management_rate_index = management_rate;
    cmd.control_rate_index = control_rate;

    sdio_send_wmi_cmd(mbox, &cmd.header);
}

uint32_t boot_channel_index = 0;
uint32_t boot_channel_wait = 0;
uint16_t requested_channel = 0;

uint8_t boot_channel_list[] = {
    1, 6, 11,
    1, 6, 11,
    1, 6, 11,
    2, 3, 4, 5,
    1, 6, 11,
    7, 8, 9, 10,
    12, 13, 14,
    0
};

void get_next_scan_channel() {
    uint32_t index = boot_channel_index;
    while(true) {
        uint8_t item = boot_channel_list[index];
        if(item == 0) {
            panic("hi");
            /*boot_channel_wait += boot_channel_wait;

            if(boot_channel_wait > 7)
                boot_channel_wait = 7;

            extern WifiAp_t access_points[6];

            for(size_t i = 0; i < 6; i++) {
                if(!(access_points[i].flags & sgWifiAp_FLAGS_ACTIVE))
                    continue;

                access_points[i].timecrt++;
                if(access_points[i].timecrt <= sgWifiAp_TIMEOUT)
                    continue;

                access_points[i].rssi = 0;
                memset(access_points[i].rssi_past, 0 , 8 * sizeof(uint8_t));
            }

            index = 0;
            continue;*/
        }

        boot_channel_index = index + 1;
        requested_channel = item;
        break;
    }
}

uint32_t channel_to_mhz(uint32_t channel) {
    channel -= 1;
    if(channel > 13)
        channel = 0;

    if(channel == 13)
        return 0x9B4;
    else
        return 0x96C + channel * 5;
}


void sdio_wmi_scan_channel(void) {
    if(ath_await_scan_complete != 0)
        return; // Already scanning

    sdio_wmi_set_bss_filter_cmd(0, BSS_FILTER_NONE, 0); // Filter Off
    print("set_bss_filter()\n");

    sdio_wmi_set_probed_ssid_cmd(0, SSID_PROBE_FLAG_DISABLE, "");
    print("set_probed_ssid()\n");

    extern uint32_t regulatory_channels;
    while((regulatory_channels & (1 << requested_channel)) == 0)
        get_next_scan_channel(); // Updates requested_channel

    extern uint16_t current_channel;
    current_channel = requested_channel;
        
    uint16_t channels[1] = {0};
    channels[0] = channel_to_mhz(current_channel);

    sdio_wmi_set_channel_params_cmd(0, 0, PHY_MODE_11G, 1, channels); print("set_channel_params()\n");

    uint32_t time = boot_channel_wait << 6;

    wmi_set_scan_params_cmd_t cmd = {0};
    cmd.fg_start_period = 0xFFFF;
    cmd.fg_end_period = 0xFFFF;
    cmd.bg_period = 0xFFFF;
    cmd.maxact_chdwell_time = time;
    cmd.pas_chdwell_time = time;
    cmd.short_scan_ratio = 0;
    cmd.scan_control_flags = 1;
    cmd.minact_chdwell_time = time;
    cmd.maxact_scan_per_ssid = 0x0;
    cmd.max_dfsch_act_time = 0x0;

    sdio_wmi_set_scan_params_cmd(0, &cmd); print("set_scan_params()\n");
    sdio_wmi_set_bss_filter_cmd(0, BSS_FILTER_ALL, 0); print("set_bss_filter()\n");

    ath_await_scan_complete = 1;

    sdio_wmi_start_scan_cmd(0, SCAN_TYPE_LONG); print("start_scan()\n");

    do {
        sdio_poll_mbox(0);
    } while(ath_await_scan_complete != 0);
}

void sdio_wmi_connect(void) {
    sdio_wmi_set_bss_filter_cmd(0, BSS_FILTER_CURRENT_BSS, 0);

    wmi_set_scan_params_cmd_t cmd = {0};
    cmd.fg_start_period = 0xFFFF;
    cmd.fg_end_period = 0xFFFF;
    cmd.bg_period = 0xFFFF;
    cmd.maxact_chdwell_time = 0xC8;
    cmd.pas_chdwell_time = 0xC8;
    cmd.short_scan_ratio = 0;
    cmd.scan_control_flags = 5;
    cmd.minact_chdwell_time = 0xC8;
    cmd.maxact_scan_per_ssid = 0x0;
    cmd.max_dfsch_act_time = 0x0;

    sdio_wmi_set_scan_params_cmd(0, &cmd);

    extern bool ap_found;
    extern uint8_t ap_index;
    extern WifiAp_t access_points[6];

    extern uint16_t current_channel;

    requested_channel = access_points[ap_index].channel;
    current_channel = access_points[ap_index].channel;

    channel_to_mhz(current_channel);

    uint16_t channels[1] = {0};
    channels[0] = channel_to_mhz(current_channel);

    sdio_wmi_set_channel_params_cmd(0, 0, PHY_MODE_11G, 1, channels);

    // TODO(thom_tl): Don't assume the network is open

    sdio_wmi_set_bitrate_cmd(0, 0xFF, 0, 0);
    sdio_wmi_set_framerates_cmd(0, 1, 0xA4, 0xFFF7);
    sdio_wmi_synchronize_cmd(0, 0x2008, 0);
    sdio_wmi_set_power_mode_cmd(0, 2);
    sdio_wmi_synchronize_cmd(0, 0, 0);

    wmi_create_pstream_cmd_t pstream = {0};
    pstream.min_service_int = 0x14;
    pstream.max_service_int = 0x14;
    pstream.inactivity_int = 0x98967F;
    pstream.suspension_time = 0xFFFFFFFF;
    pstream.service_start_time = 0;
    pstream.min_data_rate = 0x14500;
    pstream.mean_data_rate = 0x14500;
    pstream.peak_data_rate = 0x14500;
    pstream.max_burst_size = 0;
    pstream.delay_bound = 0;
    pstream.min_phy_rate = 0x5B8D80;
    pstream.sba = 0x2000;
    pstream.medium_time = 0;
    pstream.nominal_msdu = 0x80D0;
    pstream.max_msdu = 0xD0;
    pstream.traffic_class = 0;
    pstream.traffic_direction = 0x02;
    pstream.rx_queue_num = 0xFF;
    pstream.traffic_type = 1;
    pstream.voice_ps_capability = 0;
    pstream.tsid = 5;
    pstream.user_priority = 0;

    sdio_wmi_create_pstream_cmd(0, &pstream);
    sdio_wmi_set_wsc_status_cmd(0, 0);
    sdio_wmi_set_disconnect_timeout_cmd(0, 2);
    sdio_wmi_set_keepalive_cmd(0, 0);

    ath_await_connect_complete = 1;

    wmi_connect_cmd_t connect = {0};
    connect.network_type = 1;
    connect.dot11_auth_mode = 1;
    connect.auth_mode = 1;
    connect.pairwise_crypto_type = 1;
    connect.pairwise_cypto_len = 0;
    connect.group_crypto_type = 1;
    connect.group_crypto_len = 0;
    connect.ssid_length = access_points[ap_index].ssid_len;
    memcpy(connect.ssid, access_points[ap_index].ssid, access_points[ap_index].ssid_len);

    connect.channel = channels[0];
    memcpy(connect.bssid, access_points[ap_index].bssid, 6);

    connect.control_flags = 0;

    sdio_wmi_connect_cmd(0, &connect);
    print("sdio_wmi_connect_cmd()\n");

    do {
        sdio_poll_mbox(0);
    } while(ath_await_connect_complete != 0);
}