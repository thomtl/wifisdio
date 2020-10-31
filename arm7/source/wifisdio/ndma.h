#pragma once

#include <nds.h>

#define NDMAGCNT (*(vu32*)(0x4004100))
#define NDMAxSAD(ch) (*(vu32*)(0x4004104 + (ch * 0x1C)))
#define NDMAxDAD(ch) (*(vu32*)(0x4004108 + (ch * 0x1C)))
#define NDMAxTCNT(ch) (*(vu32*)(0x400410C + (ch * 0x1C)))
#define NDMAxWCNT(ch) (*(vu32*)(0x4004110 + (ch * 0x1C)))
#define NDMAxBCNT(ch) (*(vu32*)(0x4004114 + (ch * 0x1C)))
#define NDMAxCNT(ch) (*(vu32*)(0x400411C + (ch * 0x1C)))

#define NDMA_DST_INC (0 << 10)
#define NDMA_DST_DEC (1 << 11)
#define NDMA_DST_FIX (2 << 10)

#define NDMA_SRC_INC (0 << 13)
#define NDMA_SRC_DEC (1 << 13)
#define NDMA_SRC_FIX (2 << 13)
#define NDMA_SRC_FILL (3 << 13)

#define NDMA_BLOCK_SIZE(n) ((n) << 16)

#define NDMA_START_SDIO (9 << 24)

#define NDMA_REPEAT_TCNT (0 << 29)
#define NDMA_REPEAT_INFINITE (1 << 29)

#define NDMA_ENABLE (1 << 31)
#define NDMA_BUSY (1 << 31)