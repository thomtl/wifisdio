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

void net_handle_packet(uint8_t* src_mac, uint8_t* data, uint16_t len);
void net_send_packet(uint16_t proto, uint8_t* dest_mac, uint8_t* data, uint16_t len);