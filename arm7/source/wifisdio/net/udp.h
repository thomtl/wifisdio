#pragma once

#include <stdint.h>
#include <stddef.h>

#include "base.h"

typedef struct {
    uint16_t source_port;
    uint16_t dest_port;
    uint16_t length;
    uint16_t checksum;
    uint8_t body[];
} __attribute__((packed)) udp_frame_t;

void udp_handle_packet(net_address_t* source, uint8_t* body, size_t len);
void udp_send_packet(net_address_t* target, uint16_t port, uint8_t* body, size_t len);

typedef void (*udp_callback_t)(net_address_t* source, uint8_t* body, size_t len);

uint16_t udp_handle_port(udp_callback_t callback, uint16_t preferred_port);