#include "wifisdio.h"
#include "sdio.h"

#include "ndma.h"

#include <stdio.h>
#include <string.h>

uint8_t sdio_access_register_core_inj(uint32_t param, uint16_t cmd) {
    SDIO_CMD_PARAM = param;
    uint32_t irq_stat = SDIO_IRQ_STAT;
    irq_stat &= ~0x83000000; // Clear bit 31, 25, 24 (error, txrq, rxrdy)
    irq_stat &= ~0x7f0000; // Clear bit 22...16 (error)
    irq_stat &= ~0x5; // Clear bit 0, 2 (dataend, cmdrespondend)
    SDIO_IRQ_STAT = irq_stat;

    SDIO_STOP_INTERNAL &= ~1;

    SDIO_CMD = cmd;

    while(1) {
        irq_stat = SDIO_IRQ_STAT;
        if(irq_stat & 0x7f0000)
            return -1;
        
        if(irq_stat & 1)
            break;
    }

    irq_stat = SDIO_IRQ_STAT;
    if(irq_stat & (1 << 22))
        return -1; // HW Timeout

    return SDIO_REPLY32 & 0xFF;
}

// SDIO_GET_RELATIVE_ADDR
uint8_t sdio_cmd3(uint32_t param) {
    return sdio_access_register_core_inj(param, 0x403); // Single Byte CMD3
}

// SDIO_SEND_OP_COND
uint8_t sdio_cmd5(uint32_t param) {
    return sdio_access_register_core_inj(param, 0x705); // Single Byte CMD5
}

// SDIO_SELECT_DESELECT_CARD
uint8_t sdio_cmd7(uint32_t param) {
    return sdio_access_register_core_inj(param, 0x507); // Single Byte CMD7
}

int sdio_cmd53_access_inj(uint32_t* xfer_buf, uint32_t param, uint32_t len) {
    SDIO_CMD_PARAM = param;

    uint32_t irq_stat = SDIO_IRQ_STAT;
    irq_stat &= ~0x83000000; // Clear bit 31, 25, 24 (error, txrq, rxrdy)
    irq_stat &= ~0x7f0000; // Clear bit 22...16 (error)
    irq_stat &= ~0x5; // Clear bit 0, 2 (dataend, cmdrespondend)
    SDIO_IRQ_STAT = irq_stat;

    SDIO_STOP_INTERNAL &= ~1;

    uint32_t blklen, numblk;
    if(param & (1 << 27)) { // Block Mode
        blklen = 0x80;
        numblk = len;
        len <<= 7;
    } else {
        blklen = len;
        numblk = 1;
    }

    SDIO_BLKLEN16 = blklen;
    SDIO_NUMBLK16 = numblk;

    #ifdef TRY_SDIO_DATA32_MODE // Disturbs HW even when in DATA16 mode
    SDIO_BLKLEN32 = blklen;
    SDIO_NUMBLK32 = numblk;
    #endif

    #ifdef TRY_SDIO_NDMA
    if(param & (1 << 27)) { // Block Mode
        SDIO_BLKLEN32 = blklen; // Disturbs HW even when in DATA16 mode
        SDIO_NUMBLK32 = numblk;

        SDIO_DATA_CTL = 0x2; // Want DATA32
        SDIO_IRQ32 = 0x402; // Clear FIFO, DATA32 mode

        if(param & (1 << 31)) { // Write flag
            NDMAxSAD(0) = (uint32_t)xfer_buf;
            NDMAxDAD(0) = (uint32_t)&SDIO_DATA32;
        } else {
            NDMAxSAD(0) = (uint32_t)&SDIO_DATA32;
            NDMAxDAD(0) = (uint32_t)xfer_buf;
        }

        NDMAxTCNT(0) = len >> 2;
        NDMAxWCNT(0) = 0x80 / 4;
        NDMAxBCNT(0) = 0;

        uint32_t command = 0;
        command |= (2 << ((param & (1 << 31)) ? 10 : 13)); // Set SDIO_DATA32 increment mode to fixed
        command |= (5 << 16); // Physical Block Size
        command |= (9 << 24); // DSiWIFI Startup Mode
        command |= (0 << 29); // Repeat until NDMAxTCNT
        command |= (1 << 31); // Enable
        NDMAxCNT(0) = command;
    } else {
        SDIO_DATA_CTL = 0; // Want DATA16 mode
        SDIO_IRQ32 = 0x400; // Clear FIFO, DATA16 mode
    }
    #endif

    uint32_t cmd = 0;
    if(param & (1 << 31)) // Writeflag
        cmd = 0x4C35;
    else
        cmd = 0x5C35;

    if((param & (1 << 27)) && numblk != 1) // Block mode
        cmd |= 0x2000; // Multiblock, Or is that used for NDMA with FIFO32
    SDIO_CMD = cmd;

    while(1) {
        uint32_t irq_stat = SDIO_IRQ_STAT;
        if(irq_stat & 0x7f0000)
            return -1;
        
        if(irq_stat & 1)
            break;
    }

    irq_stat = SDIO_IRQ_STAT;
    if(irq_stat & (1 << 22))
        return -1; // HW Timeout

    #if defined(TRY_SDIO_DATA32_MODE) || defined(TRY_SDIO_NDMA)
    if((SDIO_DATA_CTL & 2) && (SDIO_IRQ32 & 2)) {
        // Wait for NDMA transfer to finish or an error
        while(NDMAxCNT(0) & (1 << 31)) {
            if(SDIO_IRQ_STAT & 0x7f0000)
                return -1;
        }
    } else
    #endif
    {
        if(param & (1 << 31)) { // Writeflag
            while(true) {
                uint32_t irq_stat = SDIO_IRQ_STAT;
                if(irq_stat & 0x7f0000)
                    return -1;

                if(irq_stat & 0x2000000)
                    break;
            }

            #ifdef SDIO_DMA
            uint32_t dma_len = (len + 1) >> 1; // Round up
            DMA_SRC(3) = xfer_buf;
            DMA_DEST(3) = &SDIO_DATA16;
            DMA_CR(3) = DMA_DST_FIX | DMA_16_BIT | DMA_ENABLE | dma_len;

            // TODO(thom_tl): Don't you have to wait for DMA to be done here?, wifisdio line 1334
            #else
            uint16_t* xfer = (uint16_t*)xfer_buf;
            size_t n = (len + 2 - 1) / 2;
            for(size_t i = 0; i < n; i++)
                SDIO_DATA16 = *xfer++;
            
            #endif
        } else {
            while(true) {
                uint32_t irq_stat = SDIO_IRQ_STAT;
                if(irq_stat & 0x7f0000)
                    return -1;

                if(irq_stat & 0x1000000)
                    break;
            }

            #ifdef SDIO_DMA
            uint32_t dma_len = (len + 1) >> 1; // Round up
            DMA_SRC(3) = &SDIO_DATA16;
            DMA_DEST(3) = xfer_buf;
            DMA_CR(3) = DMA_SRC_FIX | DMA_16_BIT | DMA_ENABLE | dma_len;

            // TODO(thom_tl): Don't you have to wait for DMA to be done here?, wifisdio line 1334
            #else
            uint16_t* xfer = (uint16_t*)xfer_buf;
            size_t n = (len + 2 - 1) / 2;
            for(size_t i = 0; i < n; i++)
                *xfer++ = SDIO_DATA16;
            
            #endif
        }
    }

    // Wait for Data End, apparently works better with it
    while(true) {
        uint32_t irq_stat = SDIO_IRQ_STAT;
        if(irq_stat & 0x7f0000)
            return -1;

        if(irq_stat & 0x4)
            break;
    }
    
    return 0;
}

void sdio_cmd53_write(uint32_t* xfer_buf, uint32_t dst, size_t len) {
    // Isolate func + blockmode, bit26 incrementing, bit 31 write
    uint32_t param = 0;

    param |= (dst << 9); // Move addr to bit 9 - 25
    param |= (dst & ((7 << 28) | (1 << 27))); // Isolate Func + Blockmode
    param |= (1 << 26); // Set Incrementing
    param |= (1 << 31); // Set Write
    param |= (len & ~0x200); // Crop len and write to bit 0 - 8
    
    sdio_cmd53_access_inj(xfer_buf, param, len);
}

void sdio_cmd53_read(uint32_t* xfer_buf, uint32_t src, size_t len) {
    // Isolate func + blockmode, bit26 incrementing, bit 31 write
    uint32_t param = 0;

    param |= (src << 9); // Move addr to bit 9 - 25
    param |= (src & ((7 << 28) | (1 << 27))); // Isolate Func + Blockmode
    param |= (1 << 26); // Set Incrementing
    param |= (len & ~0x200); // Crop len and write to bit 0 - 8
    
    sdio_cmd53_access_inj(xfer_buf, param, len);
}

// TODO(thom_tl): These {read, write}_func_{byte, word} functions could probably be a tad more efficient by using CMD52 for function 0 instead of CMD53
void sdio_write_func_word(uint8_t func, uint32_t addr, uint32_t data) {
    uint32_t xfer_buf[0x80] = {0}; // TODO: what length?
    xfer_buf[0] = data;

    uint32_t dst = addr | (func << 28); // Dest + func1
    uint32_t len = 4;

    sdio_cmd53_write(xfer_buf, dst, len);
}

uint32_t sdio_read_func_word(uint8_t func, uint32_t addr) {
    uint32_t xfer_buf[0x80] = {0}; // TODO: what length?

    uint32_t dst = addr | (func << 28); // Addr + func1
    uint32_t len = 4; // len
    sdio_cmd53_read(xfer_buf, dst, len);

    return xfer_buf[0];
}

void sdio_write_func_byte(uint8_t func, uint32_t addr, uint8_t data) {
    uint32_t xfer_buf[0x80] = {0}; // TODO: what length?
    xfer_buf[0] = data & 0xFF;

    uint32_t dst = addr | (func << 28); // Dest + func1
    uint32_t len = 1;

    sdio_cmd53_write(xfer_buf, dst, len);
}

uint8_t sdio_read_func_byte(uint8_t func, uint32_t addr) {
    uint32_t xfer_buf[0x80] = {0}; // TODO: what length?

    uint32_t dst = addr | (func << 28); // Addr + func1
    uint32_t len = 1; // len
    sdio_cmd53_read(xfer_buf, dst, len);

    return xfer_buf[0];
}

uint32_t sdio_read_intern_word(uint32_t addr) {
    uint32_t xfer_buf[0x80] = {0};

    // Send WINDOW_READ_ADDR
    xfer_buf[0] = addr >> 8;
    sdio_cmd53_write(xfer_buf, 0x1000047c | 1, 3); // Upper 24 bits

    xfer_buf[0] = addr & 0xFF;
    sdio_cmd53_write(xfer_buf, 0x1000047c, 1); // Lower 8 bites

    return sdio_read_func_word(1, 0x474); // Read WINDOW_DATA
}

void sdio_write_intern_word(uint32_t addr, uint32_t data) {
    uint32_t xfer_buf[0x80] = {0};

    sdio_write_func_word(1, 0x474, data); // Write WINDOW_DATA

    // Send WINDOW_WRITE_ADDR
    xfer_buf[0] = addr >> 8;
    sdio_cmd53_write(xfer_buf, 0x10000478 | 1, 3); // Upper 24 bits

    xfer_buf[0] = addr & 0xFF;
    sdio_cmd53_write(xfer_buf, 0x10000478, 1); // Lower 8 bites
}

// aka Get Host Interest Area
uint32_t sdio_vars(void) {
    // For some reason nocash wifiboot corrupts this area, so try to hardcode some sane defaults?
    //uint32_t ret = *((uint32_t*)(twlcfg_etc_buf + 0x1E4));
    uint32_t ret = 0;

    extern uint32_t chip_id; // TODO: More elegant
    switch (chip_id) {
        case 0x2000001: ret = 0x500400; break; // AR6002
        case 0xD000000: ret = 0x520000; break; // AR6013
        case 0xD000001: ret = 0x520000; break; // AR6014
        default:
            print("sdio: Unknown CHIP ID\n");
            while(1)
                ;
    }

    return ret;
}

uint32_t sdio_read_mbox_word(uint8_t mbox) {
    return sdio_read_func_word(1, FUNC1_MBOX_TOP(mbox) - 4);
}

void sdio_write_mbox_word(uint8_t mbox, uint32_t data) {
    sdio_write_func_word(1, FUNC1_MBOX_TOP(mbox) - 4, data);
}

void sdio_cmd53_read_mbox_to_xfer_buf(uint8_t mbox, uint32_t* xfer_buf, size_t len) {
    sdio_cmd53_read(xfer_buf, (0x10000000 | FUNC1_MBOX_TOP(mbox)) - len, len);
}


void sdio_check_mbox_state(uint32_t* xfer_buf) {
    sdio_cmd53_read(xfer_buf, 0x10000400, 0xC);
}

void sdio_recv_mbox_block(uint8_t mbox, uint32_t* xfer_buf) {
    uint32_t timeout = 0x1000;

    while(timeout > 0) {
        sdio_check_mbox_state(xfer_buf);
        uint8_t state = ((uint8_t*)xfer_buf)[0];
        if((state & (1 << mbox)) != 0) { // mboxN not empty
            sdio_cmd53_read(xfer_buf, (0x18000000 | FUNC1_MBOX_TOP(mbox)) - 0x80, 1);
            return;
        }

        timeout--;
    }

    // Timeout
    return;
}

void sdio_send_mbox_block(uint8_t mbox, uint8_t* xfer_buf, uint8_t* src) {
    uint32_t* tmp = (uint32_t*)xfer_buf;

    uint16_t len = ((uint16_t*)src)[1] + 6;
    if((uintptr_t)xfer_buf != (uintptr_t)src)
        memcpy(xfer_buf, src, len);
    
    xfer_buf += len;
    src += len;

    memset(xfer_buf, 0, (-len) & 0x7F);
    len += 0x7F;
    len &= ~0x7F;
    sdio_cmd53_write(tmp, (0x18000000 | FUNC1_MBOX_TOP(mbox)) - len, len >> 7); // Length to 0x80 blocks
}