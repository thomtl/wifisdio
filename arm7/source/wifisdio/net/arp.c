#include "arp.h"

#include "../wifisdio.h"
#include "../wifi.h"

#include "base.h"

#include <string.h>
#include <nds.h>

TWL_BSS arp_cache_entry_t arp_cache[ARP_CACHE_LEN];

int update_arp_cache(uint16_t hwtype, arp_ipv4_t* data) {
    for(size_t i = 0; i < ARP_CACHE_LEN; i++) {
        if(arp_cache[i].state == ARP_CACHE_FREE)
            continue;

        if(arp_cache[i].hw_type == hwtype && arp_cache[i].ip == data->source_ip) {
            memcpy(arp_cache[i].mac, data->source_mac, 6);
            return 1;
        }
    }

    return -1;
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

void arp_handle_packet(uint8_t* data, uint16_t len) {
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
    extern uint8_t device_mac[6];
    //if(memcmp(device_mac, arp_data->dest_mac, 6) != 0)


    if(!merge && insert_arp_cache(req->hw_type, arp_data))
        panic("arp: No space in ARP cache\n");

    switch (req->op)
    {
    case ARP_OP_REQUEST: {
        memcpy(arp_data->dest_mac, arp_data->source_mac, 6);
        arp_data->dest_ip = arp_data->source_ip;

        memcpy(arp_data->source_mac, device_mac, 6);
        arp_data->source_ip = 0x0;

        req->op = ARP_OP_REPLY;

        req->op = htons(req->op);
        req->hw_type = htons(req->hw_type);
        req->proto_type = htons(req->proto_type);

        net_send_packet(PROTO_ARP, arp_data->dest_mac, req, sizeof(arp_request_t) + sizeof(arp_ipv4_t));
        break;
    }
        
    default:
        panic("arp: Unknown op %x\n", req->op);
        break;
    }
}