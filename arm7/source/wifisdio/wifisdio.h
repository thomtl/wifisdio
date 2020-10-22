#pragma once

// Options

//#define TRY_SDIO_DATA32_MODE
#define WIFI_WITH_DSI_IRQ
#define TRY_SDIO_NDMA
//#define MANUAL_SDIO_NDMA_START
//#define TRY_SDIO_DATA32_MODE

//#define SDIO_DMA

extern void print(const char* s);

void sdio_atheros_init(void);