#pragma once

#include <stdint.h>
#include <stddef.h>

#include "base.h"

typedef struct {
    uint16_t hw_type;
    uint16_t proto_type;
    uint8_t hw_addr_len;
    uint8_t proto_addr_len;
    uint16_t op;
    uint8_t body[];
} __attribute__((packed)) arp_request_t;

typedef struct {
    uint8_t source_mac[6];
    uint32_t source_ip;
    uint8_t dest_mac[6];
    uint32_t dest_ip;
} __attribute__((packed)) arp_ipv4_t;

#define ARP_HWTYPE_ETHERNET 1


#define ARP_OP_REQUEST 1
#define ARP_OP_REPLY 2

#define ARP_CACHE_LEN 32
#define ARP_CACHE_FREE 0
#define ARP_CACHE_WAITING 1
#define ARP_CACHE_RESOLVED 2

typedef struct {
    uint16_t hw_type;
    uint8_t state;
    uint8_t mac[6];
    uint32_t ip;
} __attribute__((packed)) arp_cache_entry_t;

void arp_handle_packet(net_address_t* source, uint8_t* data, uint16_t len);

void arp_request(uint32_t ip);
uint8_t* arp_lookup(uint32_t ip);