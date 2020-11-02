#pragma once

#include <stdint.h>
#include <stddef.h>

#include "base.h"

typedef struct {
    uint8_t type;
    uint8_t tos;
    uint16_t len;
    uint16_t id;
    uint16_t frag;
    uint8_t ttl;
    uint8_t proto;
    uint16_t checksum;
    uint32_t source_ip;
    uint32_t dest_ip;
    uint8_t body[];
} __attribute__((packed)) ipv4_frame_t;

#define IPv4_VERSION 4

#define IPv4_PROTO_ICMP 0x1
#define IPv4_PROTO_TCP 0x6
#define IPv4_PROTO_UDP 0x11

uint16_t ipv4_checksum(void* addr, size_t count);

void ipv4_handle_packet(net_address_t* source, uint8_t* data, uint16_t len);
void ipv4_send_packet(net_address_t* target, uint8_t proto, uint8_t* data, size_t data_len);