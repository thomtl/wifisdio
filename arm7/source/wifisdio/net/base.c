#include "../wifisdio.h"
#include "base.h"

#include "../wifi.h"
#include "../wmi.h"

#include "arp.h"

#include <stdlib.h>
#include <string.h>

void net_handle_packet(uint8_t* src_mac, uint8_t* data, uint16_t len) {
    llc_snap_frame_t* frame = (llc_snap_frame_t*)data;

    if(frame->protocol == htons(PROTO_ARP))
        arp_handle_packet(frame->body, len - sizeof(llc_snap_frame_t));
    else
        print("net: Unknown proto %x\n", htons(frame->protocol));
    /*else if(frame->protocol == htons(0x806))
        print("IPv4\n");
    else
        print("\n");*/
}

void net_send_packet(uint16_t proto, uint8_t* dest_mac, uint8_t* data, uint16_t len) {
    typedef struct {
        wmi_mbox_data_send_header_t header;
        llc_snap_frame_t llc;
    } __attribute__((packed)) frame_t;

    frame_t* frame = malloc(sizeof(frame_t) + len);
    memset(frame, 0, sizeof(frame_t) + len);

    frame->llc.llc_snap[0] = 0xAA;
    frame->llc.llc_snap[1] = 0xAA;
    frame->llc.llc_snap[2] = 0x3;

    frame->llc.protocol = htons(proto);

    memcpy(frame->llc.body, data, len);

    sdio_tx_packet(dest_mac, &frame->header, sizeof(frame_t) + len);
}