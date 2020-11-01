#include "ipv4.h"

#include "../wifisdio.h"
#include "../wifi.h"

#include "icmp.h"
#include "arp.h"
#include "base.h"
#include "net_alloc.h"

#include <string.h>

uint16_t ipv4_checksum(void* addr, size_t count) {
    uint32_t sum = 0;
    uint16_t* ptr = addr;

    for(; count > 1; count -= 2)
        sum += *ptr++;

    if(count > 0)
        sum += *(uint8_t*)ptr;

    while (sum >> 16)
        sum = (sum & 0xFFFF) + (sum >> 16);

    return ~sum;
}

void ipv4_handle_packet(uint8_t* data, uint16_t len) {
    ipv4_frame_t* frame = (ipv4_frame_t*)data;
    
    if((frame->type >> 4) != IPv4_VERSION)
        panic("ipv4_handle_packet(): Invalid frame version 0x%x\n", (frame->type & 0xF));

    if((frame->type & 0xF) < 5)
        panic("ipv4_handle_packet(): IPv4 Header must be at least 5, but is 0x%x\n", (frame->type >> 4));

    if(frame->ttl == 0)
        panic("ipv4_handle_packet(): Time to live is 0\n");

    if(ipv4_checksum(frame, (frame->type & 0xF) * 4))
        return; // Invalid checksum

    frame->source_ip = htonl(frame->source_ip);
    frame->dest_ip = htonl(frame->dest_ip);
    frame->len = htons(frame->len);
    frame->id = htons(frame->id);

    size_t body_len = frame->len - ((frame->type & 0xF) * 4);
    if(frame->proto == IPv4_PROTO_ICMP)
        icmp_handle_packet(frame, frame->body, body_len);
    else
        print("ipv4: Unknown proto 0x%x\n", frame->proto);
}

void ipv4_send_packet(uint32_t dest_ip, uint8_t proto, uint8_t* data, size_t data_len) {
    size_t total_len = sizeof(ipv4_frame_t) + data_len;
    
    ipv4_frame_t* frame = net_malloc(total_len);
    if(!frame)
        panic("ipv4_send_packet(): Couldn't malloc frame of size 0x%x\n", total_len);
    memset(frame, 0, total_len);

    frame->type = 0x45;
    frame->tos = 0;
    frame->ttl = 64;
    frame->frag = htons(1 << 14);

    frame->proto = proto;

    static int id = 0;
    frame->id = id++;

    extern uint32_t device_ip;
    frame->source_ip = device_ip;
    frame->dest_ip = htonl(dest_ip);
    frame->len = htons(total_len);

    frame->checksum = ipv4_checksum(frame, 5 * 4);
    memcpy(frame->body, data, data_len);

    uint8_t* mac = arp_lookup(dest_ip);
    if(!mac) {
        arp_request(dest_ip);
        net_free(frame);

        return;
    }

    net_send_packet(PROTO_IPv4, mac, frame, total_len);

    net_free(frame);
}