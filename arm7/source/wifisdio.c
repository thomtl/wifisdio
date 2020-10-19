#include "wifisdio.h"
#include "ndma.h"

#include <stdio.h>

// Options

//#define TRY_SDIO_DATA32_MODE
#define WIFI_WITH_DSI_IRQ
#define TRY_SDIO_NDMA
//#define MANUAL_SDIO_NDMA_START
//#define TRY_SDIO_DATA32_MODE

//#define SDIO_DMA

// Globals
uint32_t chip_id;

void sdio_controller_init(void) {
    SDIO_SOFT_RESET &= ~0b11; // Software reset, bit 1 shouldn't have any effect but FW does it as well 
    SDIO_SOFT_RESET |= 0b11;

    uint16_t stop = SDIO_STOP_INTERNAL;
    stop |= (1 << 8);
    stop &= ~0b1;
    SDIO_STOP_INTERNAL = stop;

    SDIO_CARD_OPTION = 0x80D0;
    SDIO_CARD_CLK_CTL = 0x40;

    uint16 option = SDIO_CARD_OPTION;
    option |= (1 << 15);
    option &= ~(1 << 15);
    SDIO_CARD_OPTION = option;

    SDIO_CARD_OPTION |= (1 << 8);
    SDIO_CARD_OPTION &= ~(1 << 8);

    SDIO_CARD_CLK_CTL = 0x100;

    #ifdef TRY_SDIO_DATA32_MODE
    SDIO_DATA_CTL = 0x2; // DATA32 Mode
    SDIO_IRQ32 = 0x402; // Clear FIFO, DATA32
    #endif

    #ifdef WIFI_WITH_DSI_IRQ
    (void)SDIO_CARD_IRQ_STAT; // Read to ACK
    SDIO_CARD_IRQ_MASK &= ~1; // Unmask IRQ
    SDIO_CARD_IRQ_CTL |= 1; // Enable IRQ
    #endif

    GPIO_WIFI &= ~(1 << 8); // Tell HW we're using DSiWifi and not NDSWifi

    i2cWriteRegister(I2C_PM, 0x30, 0x13); // Enable WIFISDIO + WiFi LED blink on transfer
}

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

    if((param & (1 << 27)) && numblk == 1) // Block mode
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

void sdio_cmd53_write(uint32_t* xfer_buf, uint32_t dst, uint32_t len) {
    // Isolate func + blockmode, bit26 incrementing, bit 31 write
    uint32_t param = 0;

    param |= (dst << 9); // Move addr to bit 9 - 25
    param |= (dst & ((7 << 28) | (1 << 27))); // Isolate Func + Blockmode
    param |= (1 << 26); // Set Incrementing
    param |= (1 << 31); // Set Write
    param |= (len & ~0x200); // Crop len and write to bit 0 - 8
    
    sdio_cmd53_access_inj(xfer_buf, param, len);
}

void sdio_cmd53_read(uint32_t* xfer_buf, uint32_t dst, uint32_t len) {
    // Isolate func + blockmode, bit26 incrementing, bit 31 write
    uint32_t param = 0;

    param |= (dst << 9); // Move addr to bit 9 - 25
    param |= (dst & ((7 << 28) | (1 << 27))); // Isolate Func + Blockmode
    param |= (1 << 26); // Set Incrementing
    param |= (len & ~0x200); // Crop len and write to bit 0 - 8
    
    sdio_cmd53_access_inj(xfer_buf, param, len);
}

uint8_t sdio_read_register(uint32_t addr) {
    uint32_t param = (addr << 9) | (addr & (7 << 28));
    
    return sdio_access_register_core_inj(param, 0x434); // Single Byte CMD52
}

void sdio_write_register(uint32_t addr, uint8_t data) {
    uint32_t param = (data | (addr << 9)) | (addr & (7 << 28)) | (1 << 31); // Data + Addr + Writeflag

    sdio_access_register_core_inj(param, 0x434); // Single Byte CMD52
}

uint8_t sdio_read_func0byte(uint32_t addr) {
    return sdio_read_register(addr);
}

void sdio_write_func0byte(uint32_t addr, uint8_t data) {
    sdio_write_register(addr, data);
}

void sdio_write_func1word(uint32_t addr, uint32_t data) {
    uint32_t xfer_buf[0x80] = {0}; // TODO: what length?
    xfer_buf[0] = data;

    uint32_t dst = addr | (1 << 28); // Dest + func1
    uint32_t len = 4;

    sdio_cmd53_write(xfer_buf, dst, len);
}

uint32_t sdio_read_func1word(uint32_t addr) {
    uint32_t xfer_buf[0x80] = {0}; // TODO: what length?

    uint32_t dst = addr | (1 << 28); // Addr + func1
    uint32_t len = 4; // len
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

    return sdio_read_func1word(0x474); // Read WINDOW_DATA
}

int sdio_init_opcond_if_needed(void) {
    (void)sdio_read_func0byte(FUNC0_CCCR_IRQ_FLAGS);

    // If it passed okay, skip cmd5, etc
    if((SDIO_IRQ_STAT & 0x400000) == 0)
        return 0; // Need Upload

    uint32_t known_voltages = 0;
    while(true) {
        known_voltages &= ~0x100000; // bit20: 3.2V..3.3V

        sdio_cmd5(known_voltages); // IO_SEND_OP_COND

        if((SDIO_IRQ_STAT & 0x400000) != 0) {
            known_voltages = 0;
            continue;
        }

        known_voltages = SDIO_REPLY32;

        if((known_voltages & 0x80000000) == 0) // Check Ready Bit
            continue; 

        break;
    }

    sdio_cmd3(0);
    uint32_t rca = (SDIO_REPLY32 >> 16) << 16; // Isolate RCA
    sdio_cmd7(rca);

    return 1; // Need Upload
}

void sdio_init_func0(void) {
    sdio_write_func0byte(FUNC0_CCCR_POWER_CONTROL, 0x02); // CCCR Power Control
    sdio_write_func0byte(FUNC0_CCCR_BUS_INTERFACE, 0x82); // CCCR Bus Interface
    sdio_write_func0byte(FUNC0_CCCR_CARD_CAPS, 0x17); // CCCR Card Capabilities
    sdio_write_func0byte(FUNC0_FBRn_BLOCK_SIZE_LOW(1), 0x80); // FBR1 Block Size lsb
    sdio_write_func0byte(FUNC0_FBRn_BLOCK_SIZE_HIGH(1), 0x00); // FBR1 Block Size msb
    sdio_write_func0byte(FUNC0_CCCR_FUNCTION, 0x02); // Function Enable

    while(sdio_read_func0byte(FUNC0_CCCR_FUNCTION + 1) != 0x2) // Function Ready
        ;

    sdio_write_func1word(0x418, 0); // Func1 IRQ enable
    chip_id = sdio_read_intern_word(0x000040ec);
}

extern void print(const char* c);

void sdio_atheros_init(void) {
    uint8_t wifiboard_version;
    readFirmware(0x1FD, &wifiboard_version, 1);

    sdio_controller_init(); print("sdio_controller_init()\n");
    int need_upload = sdio_init_opcond_if_needed(); print("sdio_init_opcond_if_needed()\n");
    sdio_init_func0(); print("sdio_init_func0()\n");

    char s[100] = {0};
    sprintf(s, "sdio: Board Version: v%d\n", wifiboard_version);
    print(s);

    sprintf(s, "sdio: Chip ID: 0x%lx\n", chip_id);
    print(s);
}