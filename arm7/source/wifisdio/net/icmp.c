#include "icmp.h"

#include "../wifisdio.h"
#include "../wifi.h"

void icmp_handle_packet(ipv4_frame_t* header, uint8_t* body, size_t body_len) {
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
        ipv4_send_packet(header->source_ip, IPv4_PROTO_ICMP, frame, body_len);
        break;
    }

    default:
        print("icmp: Unknown type 0x%x\n", frame->type);
        break;
    }
}