#pragma once

#include <stdint.h>
#include <stddef.h>

typedef struct {
    char ssid[33];
    uint8_t ssid_len;
    uint8_t bssid[6];
    uint8_t mac[6];
    uint16_t maxrate;
    uint32_t timecrt;
    uint16_t rssi;
    uint16_t flags;
    //uint32_t spinlock;
    uint8_t channel;
    uint8_t rssi_past[8];
    uint8_t base_rates[16];
} sgWifiAp_t;

typedef struct {
    uint8_t sa[6];
    uint8_t bssid[6];
    uint8_t strength;
    uint8_t reserved[3];
} sgBeacon_t;

#define sgWifiAp_FLAGS_ADHOC (1 << 0)
#define sgWifiAp_FLAGS_WEP (1 << 1)
#define sgWifiAp_FLAGS_WPA (1 << 2)
#define sgWifiAp_FLAGS_COMPATIBLE (1 << 3)
#define sgWifiAp_FLAGS_EXTCOMPATIBLE (1 << 4)
#define sgWifiAp_FLAGS_SHORT_PREAMBLE (1 << 5)
#define sgWifiAp_FLAGS_ACTIVE (1 << 15)

#define sgWifiAp_TIMEOUT 3