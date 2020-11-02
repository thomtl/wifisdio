#include "arp.h"

#include "../wifisdio.h"
#include "../wifi.h"

#include "base.h"

#include <string.h>
#include <nds.h>

TWL_BSS arp_cache_entry_t arp_cache[ARP_CACHE_LEN] = {0};

int update_arp_cache(uint16_t hwtype, arp_ipv4_t* data) {
    for(size_t i = 0; i < ARP_CACHE_LEN; i++) {
        if(arp_cache[i].state == ARP_CACHE_FREE)
            continue;

        if(arp_cache[i].hw_type == hwtype && arp_cache[i].ip == data->source_ip) {
            memcpy(arp_cache[i].mac, data->source_mac, 6);
            return 1;
        }
    }

    return 0;
}

int insert_arp_cache(uint16_t hwtype, arp_ipv4_t* data) {
    for(size_t i = 0; i < ARP_CACHE_LEN; i++) {
        if(arp_cache[i].state == ARP_CACHE_FREE) {
            arp_cache[i].state = ARP_CACHE_RESOLVED;

            arp_cache[i].hw_type = hwtype;
            arp_cache[i].ip = data->source_ip;
            memcpy(arp_cache[i].mac, data->source_mac, 6);

            return 0;
        }
    }

    return -1;
}

void arp_handle_packet(net_address_t* source, uint8_t* data, uint16_t len) {
    arp_request_t* req = (arp_request_t*)data;

    req->op = htons(req->op);
    req->hw_type = htons(req->hw_type);
    req->proto_type = htons(req->proto_type);

    if(req->hw_type != ARP_HWTYPE_ETHERNET)
        panic("arp: Unknown HW Type %x\n", req->hw_type);

    if(req->proto_type != PROTO_IPv4)
        panic("arp: Unknown Proto Type %x\n", req->proto_type);

    arp_ipv4_t* arp_data = (arp_ipv4_t*)req->body;

    int merge = update_arp_cache(req->hw_type, arp_data);
    extern uint32_t device_ip;
    if(arp_data->dest_ip != device_ip)
        return; // Not for us

    if(!merge && insert_arp_cache(req->hw_type, arp_data) != 0)
        panic("arp: No space in ARP cache\n");

    switch (req->op)
    {
    case ARP_OP_REQUEST: {
        memcpy(arp_data->dest_mac, arp_data->source_mac, 6);
        arp_data->dest_ip = arp_data->source_ip;

        extern uint8_t device_mac[6];
        memcpy(arp_data->source_mac, device_mac, 6);
        arp_data->source_ip = device_ip;

        req->op = ARP_OP_REPLY;

        req->op = htons(req->op);
        req->hw_type = htons(req->hw_type);
        req->proto_type = htons(req->proto_type);

        net_address_t target = {0};
        memcpy(target.mac, arp_data->dest_mac, 6);

        net_send_packet(PROTO_ARP, &target, req, sizeof(arp_request_t) + sizeof(arp_ipv4_t));
        break;
    }
    case ARP_OP_REPLY: break; // Just ignore replies here, already handled above

    default:
        print("arp: Unknown op %x\n", req->op); // TODO(thom_tl): Is this Reverse ARP? Should we handle that?
        break;
    }
}

void arp_request(uint32_t ip) {
    typedef struct {
        arp_request_t req;
        arp_ipv4_t ip;
    } __attribute__((packed)) request_t;

    request_t req = {0};

    req.req.op = htons(ARP_OP_REQUEST);
    req.req.hw_type = htons(ARP_HWTYPE_ETHERNET);
    req.req.proto_type = htons(PROTO_IPv4);
    req.req.hw_addr_len = 6;
    req.req.proto_addr_len = 4;

    req.ip.dest_ip = htonl(ip);

    extern uint8_t device_mac[6];
    memcpy(req.ip.source_mac, device_mac, 6);
    extern uint32_t device_ip;
    req.ip.source_ip = device_ip;

    uint8_t broadcast_mac[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    net_address_t target = {0};
    memcpy(target.mac, broadcast_mac, 6);
    
    net_send_packet(PROTO_ARP, &target, &req, sizeof(request_t));
}

uint8_t* arp_lookup(uint32_t ip) {
    ip = htonl(ip);
    for(size_t i = 0; i < ARP_CACHE_LEN; i++)
        if(arp_cache[i].state == ARP_CACHE_RESOLVED)
            if(arp_cache[i].ip == ip)
                return arp_cache[i].mac;

    return NULL;
}