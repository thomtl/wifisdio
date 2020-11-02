#include "icmp.h"

#include "../wifisdio.h"
#include "../wifi.h"

#include <string.h>

void icmp_handle_packet(net_address_t* source, uint8_t* body, size_t body_len) {
    icmpv4_frame_t* frame = (icmpv4_frame_t*)body;

    if(ipv4_checksum(body, body_len))
        return; // Invalid checksum

    switch (frame->type) {
    case ICMPv4_TYPE_ECHO_REQUEST: {
        icmpv4_echo_request_t* echo = (icmpv4_echo_request_t*)frame->body;
        print("icmp: Ping icmp_seq=%d\n", htons(echo->seq));
        frame->type = ICMPv4_TYPE_ECHO_REPLY;

        frame->checksum = 0;
        frame->checksum = ipv4_checksum(frame, body_len);

        net_address_t target = {0};
        memcpy(target.mac, source->mac, 6);
        target.ip = source->ip;        

        ipv4_send_packet(&target, IPv4_PROTO_ICMP, frame, body_len);
        break;
    }

    default:
        print("icmp: Unknown type 0x%x\n", frame->type);
        break;
    }
}