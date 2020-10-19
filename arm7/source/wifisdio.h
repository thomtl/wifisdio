#pragma once

#include <nds.h>

#define SDIO_BASE 0x4004A00


#define SDIO_CMD (*(vu16*)(SDIO_BASE + 0x00))
#define SDIO_CMD_PARAM (*(vu32*)(SDIO_BASE + 0x04))

#define SDIO_STOP_INTERNAL (*(vu16*)(SDIO_BASE + 0x08))
#define SDIO_NUMBLK16 (*(vu16*)(SDIO_BASE + 0x0A))
#define SDIO_REPLY32 (*(vu32*)(SDIO_BASE + 0x0C))

#define SDIO_IRQ_STAT (*(vu32*)(SDIO_BASE + 0x1C))
#define SDIO_CARD_CLK_CTL (*(vu16*)(SDIO_BASE + 0x24))
#define SDIO_BLKLEN16 (*(vu16*)(SDIO_BASE + 0x26))
#define SDIO_CARD_OPTION (*(vu16*)(SDIO_BASE + 0x28))
#define SDIO_DATA16 (*(vu16*)(SDIO_BASE + 0x30))

#define SDIO_CARD_IRQ_CTL (*(vu16*)(SDIO_BASE + 0x34))
#define SDIO_CARD_IRQ_STAT (*(vu16*)(SDIO_BASE + 0x36))
#define SDIO_CARD_IRQ_MASK (*(vu16*)(SDIO_BASE + 0x38))

#define SDIO_DATA_CTL (*(vu16*)(SDIO_BASE + 0xD8))
#define SDIO_SOFT_RESET (*(vu16*)(SDIO_BASE + 0xE0))
#define SDIO_IRQ32 (*(vu32*)(SDIO_BASE + 0x100))

#define SDIO_BLKLEN32 (*(vu32*)(SDIO_BASE + 0x104))
#define SDIO_NUMBLK32 (*(vu32*)(SDIO_BASE + 0x108))
#define SDIO_DATA32 (*(vu32*)(SDIO_BASE + 0x10C))

#define FUNC0_CCCR_FUNCTION (0x2)
#define FUNC0_CCCR_IRQ_FLAGS (0x4)
#define FUNC0_CCCR_BUS_INTERFACE (0x7)
#define FUNC0_CCCR_CARD_CAPS (0x8)
#define FUNC0_CCCR_POWER_CONTROL (0x12)

#define FUNC0_FBRn_BLOCK_SIZE_LOW(n) (((n) * 0x100) + 0x10)
#define FUNC0_FBRn_BLOCK_SIZE_HIGH(n) (((n) * 0x100) + 0x11)



#define GPIO_BASE 0x4004C00
#define GPIO_WIFI (*(vu16*)(GPIO_BASE + 0x4))

void sdio_atheros_init(void);