#pragma once

#include <stdint.h>

// Options

//#define TRY_SDIO_DATA32_MODE
#define WIFI_WITH_DSI_IRQ
#define TRY_SDIO_NDMA
//#define MANUAL_SDIO_NDMA_START
//#define TRY_SDIO_DATA32_MODE

//#define SDIO_DMA

extern void print(const char* s, ...);
extern void panic(const char* s, ...);

extern uint32_t sdio_xfer_buf[];

void sdio_init(void);