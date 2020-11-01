#pragma once

#include <stdint.h>
#include <stddef.h>

#include "ipv4.h"

typedef struct {
    uint8_t type;
    uint8_t code;
    uint16_t checksum;
    uint8_t body[];
} __attribute__((packed)) icmpv4_frame_t;

typedef struct {
    uint16_t id;
    uint16_t seq;
    uint8_t body[];
} __attribute__((packed)) icmpv4_echo_request_t;

#define ICMPv4_TYPE_ECHO_REPLY 0
#define ICMPv4_TYPE_ECHO_REQUEST 8

void icmp_handle_packet(ipv4_frame_t* header, uint8_t* body, size_t body_len);