#pragma once

#include <stdint.h>
#include <stddef.h>
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

// Use larger MBOX aliases
#define FUNC1_MBOX_BASE(n) (0x800 + ((n) * 0x800))
#define FUNC1_MBOX_LEN (0x800)
#define FUNC1_MBOX_TOP(n) (FUNC1_MBOX_BASE((n)) + FUNC1_MBOX_LEN)

#define FUNC1_INT_STATUS_ENABLE (0x418)

#define IRQ_SDIOWIFI (1 << 11)


#define GPIO_BASE 0x4004C00
#define GPIO_WIFI (*(vu16*)(GPIO_BASE + 0x4))

uint8_t sdio_cmd3(uint32_t param);
uint8_t sdio_cmd5(uint32_t param);
uint8_t sdio_cmd7(uint32_t param);

void sdio_cmd53_write(uint32_t* xfer_buf, uint32_t dst, size_t len);
void sdio_cmd53_read(uint32_t* xfer_buf, uint32_t src, size_t len);

uint8_t sdio_read_func_byte(uint8_t func, uint32_t addr);
void sdio_write_func_byte(uint8_t func, uint32_t addr, uint8_t data);

uint32_t sdio_read_func_word(uint8_t func, uint32_t addr);
void sdio_write_func_word(uint8_t func, uint32_t addr, uint32_t data);

uint32_t sdio_read_intern_word(uint32_t addr);
void sdio_write_intern_word(uint32_t addr, uint32_t data);

uint32_t sdio_vars(void);

void sdio_write_mbox_word(uint8_t mbox, uint32_t data);
uint32_t sdio_read_mbox_word(uint8_t mbox);
void sdio_cmd53_read_mbox_to_xfer_buf(uint8_t mbox, size_t len);

void sdio_recv_mbox_block(uint8_t mbox);
void sdio_send_mbox_block(uint8_t mbox, uint8_t* src);

void sdio_check_mbox_state(void);