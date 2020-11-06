#include "udp.h"

#include "../wifi.h"
#include "../wifisdio.h"

#include "net_alloc.h"
#include "ipv4.h"

#include <nds.h>
#include <string.h>

typedef struct {
    uint16_t port;
    udp_callback_t callback;
} __attribute__((packed)) udp_port_t;

#define UDP_PORT_LIST_LEN 2

TWL_BSS udp_port_t udp_ports[UDP_PORT_LIST_LEN] = {0};
uint16_t user_free_port = 1024;

uint16_t udp_handle_port(udp_callback_t callback, uint16_t preferred_port) {
    uint16_t port = 0;
    if(preferred_port == 0) {    
        port = user_free_port++;

        if(user_free_port == 0)
            panic("udp: User ports overflow\n");
    } else {
        // If we do have a preferred port check if it is used
        for(size_t i = 0; i < UDP_PORT_LIST_LEN; i++)
            if(udp_ports[i].port == preferred_port)
                panic("udp: Port %d is already used\n", preferred_port);

        port = preferred_port;
    }

    for(size_t i = 0; i < UDP_PORT_LIST_LEN; i++){
        if(udp_ports[i].port == 0) { // If it is free
            udp_ports[i].port = port;
            udp_ports[i].callback = callback;

            return port;
        }
    }

    panic("udp: Out of free port list entries\n");
    return 0;
}

void udp_handle_packet(net_address_t* source, uint8_t* body, size_t len) {
    udp_frame_t* frame = (udp_frame_t*)body;

    // TODO(thom_tl): Check frame->checksum

    frame->source_port = htons(frame->source_port);
    frame->dest_port = htons(frame->dest_port);
    frame->length = htons(frame->length);
    frame->checksum = htons(frame->checksum);

    source->port = frame->source_port;

    for(size_t i = 0; i < UDP_PORT_LIST_LEN; i++) {
        if(udp_ports[i].port == frame->dest_port) {
            if(!udp_ports[i].callback)
                panic("udp_handle_packet(): callback is null\n");
            
            return udp_ports[i].callback(source, frame->body, frame->length - sizeof(udp_frame_t));
        }
    }


    // TODO(thom_tl): Adding a second format thingy here breaks it??
    print("udp: Unhandled packet to port %d\n", frame->dest_port);
}

void udp_send_packet(net_address_t* target, uint16_t port, uint8_t* body, size_t len) {
    uint16_t total_len = sizeof(udp_frame_t) + len;
    
    udp_frame_t* frame = net_malloc(total_len);
    if(!frame)
        panic("udp_send_packet(): Couldn't malloc frame of size 0x%x\n", total_len);
    memset(frame, 0, total_len);

    frame->source_port = htons(port);
    frame->dest_port = htons(target->port);
    frame->length = htons(total_len);
    frame->checksum = 0; // TODO(thom_tl): Provide this

    memcpy(frame->body, body, len);

    ipv4_send_packet(target, IPv4_PROTO_UDP, frame, total_len);

    net_free(frame);
}


