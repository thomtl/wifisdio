#include "wifisdio.h"
#include "wmi.h"

#include "sdio.h"

#include <stdio.h>

uint8_t ath_cmd_ack_pending = 0;
uint8_t ath_data_ack_pending = 0;

uint32_t ath_lookahead_value = 0;
uint8_t ath_lookahead_flag = 0;

uint32_t recent_heartbeat = 0;
uint32_t** curr_wpa_tx_callback_list_ptr = NULL;

void sdio_send_wmi_cmd_without_poll(uint8_t mbox, wmi_mbox_send_header_t* cmd) {
    sdio_send_mbox_block(mbox, (uint8_t*)cmd);
    ath_cmd_ack_pending = (cmd->flags & MBOX_SEND_FLAGS_REQUEST_ACK) ? 1 : 0;
}

void sdio_send_wmi_cmd(uint8_t mbox, wmi_mbox_send_header_t* cmd) {
    do {
        sdio_poll_mbox(mbox);
    } while (ath_cmd_ack_pending != 0);

    sdio_send_mbox_block(mbox, (uint8_t*)cmd);
    ath_cmd_ack_pending = (cmd->flags & MBOX_SEND_FLAGS_REQUEST_ACK) ? 1 : 0;
}


void sdio_heartbeat(uint8_t mbox) {
    extern uint32_t arm7_count_60hz;

    uint32_t ticks = arm7_count_60hz - recent_heartbeat;
    if(ticks < 60)
        return;

    if(ath_cmd_ack_pending != 0) // Exit if old cmd is still busy
        return;

    recent_heartbeat = arm7_count_60hz;

    print("Heartbeat\n");

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
        sdio_tx_callback();
        sdio_Wifi_Intr_TxEnd();

        if(ath_lookahead_flag == 0) {
            sdio_check_mbox_state();
            uint8_t host_int_status = ((uint8_t*)sdio_xfer_buf)[0];
            uint8_t mbox_frame = ((uint8_t*)sdio_xfer_buf)[4];
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

        if(size > 0x200) {
            leaveCriticalSection(old_ime);
            panic("sdio_poll_mbox(): TODO: Global sdio_xfer_buf\n");
        }

        sdio_cmd53_read(sdio_xfer_buf, (0x18000000 | FUNC1_MBOX_TOP(mbox)) - size, size >> 7);


        ath_lookahead_flag = 0;

        wmi_mbox_recv_header_t* header = (wmi_mbox_recv_header_t*)sdio_xfer_buf;
        if(header->flags == 0 && !(header->flags & MBOX_RECV_FLAGS_HAS_ACK)) {
            leaveCriticalSection(old_ime);
            panic("sdio_poll_mbox(): Invalid header->flags\n");
        }

        if(header->ack_len > header->len) {
            leaveCriticalSection(old_ime);
            panic("sdio_poll_mbox(): header->ack_len > header->len\n");
        }

        if(header->flags != 0 && header->ack_len != 0) {
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
                    for(size_t i = 0; i < reply->n_channels; i++) {
                        uint16_t channel = reply->channel_list[i];

                        channel -= 0x900;
                        if(channel == 0xB4) {
                            channel_mask |= (1 << 14);
                            continue;
                        }

                        channel -= 0x6C;
                        if(channel > 0x3C)
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
                case WMI_REGDOMAIN_EVENT: {
                    uint8_t* regdomain_ptr = (uint8_t*)params;
                    uint32_t regdomain = regdomain_ptr[0] | (regdomain_ptr[1] << 8) | (regdomain_ptr[2] << 16) | (regdomain_ptr[3] << 24);

                    print("Regulatory Domain: 0x%lx\n", regdomain);
                    
                    extern uint32_t regulatory_domain;
                    regulatory_domain = regdomain;
                    break;
                }
                default: {
                    leaveCriticalSection(old_ime);
                    panic("Unknown WMI Event 0x%x\n", event);
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

void sdio_wmi_start_whatever_timer_cmd(uint8_t mbox, uint32_t time) {
    wmi_start_whatever_timer_cmd_t cmd = {0};
    cmd.header.type = MBOX_SEND_TYPE_WMI;
    cmd.header.flags = MBOX_SEND_FLAGS_REQUEST_ACK;
    cmd.header.len = sizeof(wmi_start_whatever_timer_cmd_t) - 6;
    cmd.header.cmd = WMI_START_WHATEVER_TIMER_CMD;

    cmd.time = time;

    sdio_send_wmi_cmd(mbox, &cmd.header);
}