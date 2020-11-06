#pragma once

#include <stdint.h>
#include <stddef.h>

typedef struct {
    uint8_t llc_snap[6];
	uint16_t protocol;
    uint8_t body[];
} __attribute__((packed)) llc_snap_frame_t;

#define PROTO_IPv4 0x800
#define PROTO_ARP 0x806

typedef struct {
    uint8_t mac[6];
    uint32_t ip;
    uint16_t port;
} __attribute__((packed)) net_address_t;

void net_handle_packet(uint8_t* src_mac, uint8_t* data, uint16_t len);
void net_send_packet(uint16_t proto, net_address_t* target, uint8_t* data, uint16_t len);