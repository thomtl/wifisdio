#include "wifisdio.h"
#include "ndma.h"

#include <stdio.h>
#include <string.h>

// Options

//#define TRY_SDIO_DATA32_MODE
#define WIFI_WITH_DSI_IRQ
#define TRY_SDIO_NDMA
//#define MANUAL_SDIO_NDMA_START
//#define TRY_SDIO_DATA32_MODE

//#define SDIO_DMA

// Globals
uint32_t chip_id, rom_version;
uint8_t twlcfg_etc_buf[0x214] = {0};

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

uint8_t sdio_read_func1byte(uint32_t addr) {
    uint32_t xfer_buf[0x80] = {0}; // TODO: what length?

    uint32_t dst = addr | (1 << 28); // Addr + func1
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

    return sdio_read_func1word(0x474); // Read WINDOW_DATA
}

void sdio_write_intern_word(uint32_t addr, uint32_t data) {
    uint32_t xfer_buf[0x80] = {0};

    sdio_write_func1word(0x474, data); // Write WINDOW_DATA

    // Send WINDOW_READ_ADDR
    xfer_buf[0] = addr >> 8;
    sdio_cmd53_write(xfer_buf, 0x10000478 | 1, 3); // Upper 24 bits

    xfer_buf[0] = addr & 0xFF;
    sdio_cmd53_write(xfer_buf, 0x10000478, 1); // Lower 8 bites
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


// aka Get Host Interest Area
uint32_t sdio_vars(void) {
    // For some reason nocash wifiboot corrupts this area, so try to hardcode some sane defaults?
    //uint32_t ret = *((uint32_t*)(twlcfg_etc_buf + 0x1E4));
    uint32_t ret = 0;

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

// Aka check if FW is already uploaded
bool sdio_check_host_interest(void) {
    // See sdio_vars for why this is commented out
    /*uint16_t crc16 = swiCRC16(0xFFFF, twlcfg_etc_buf + 0x1E4, 0xC);
    if(crc16 != *((uint16_t*)(twlcfg_etc_buf + 0x1E2)))
        return true; // Need upload*/

    uint32_t uploaded = sdio_read_intern_word(sdio_vars() + 0x58);
    return (uploaded != 1);
}

void sdio_reset(void) {
    sdio_write_intern_word(0x4000, 0x100); // Reset control, bit 8 = reset
    swiDelay(0x10000); // Delay is needed
    sdio_read_intern_word(0x40C0); // Reset cause
}

void sdio_write_mbox0word(uint32_t data) {
    sdio_write_func1word(0xFFC, data);
}

void sdio_cmd53_read_mbox_to_xfer_buf(uint32_t* xfer_buf, uint32_t len) {
    sdio_cmd53_read(xfer_buf, 0x10001000 - len, len);
}

uint32_t sdio_read_mbox0word(void) {
    return sdio_read_func1word(0xFFC);
}

void sdio_check_mbox_state(uint32_t* xfer_buf) {
    sdio_cmd53_read(xfer_buf, 0x10000400, 0xC);
}

void sdio_recv_mbox_block(uint32_t* xfer_buf) {
    uint32_t timeout = 0x1000;

    while(timeout >= 0) {
        sdio_check_mbox_state(xfer_buf);
        uint8_t state = ((uint8_t*)xfer_buf)[0];
        if((state & 1) == 0) { // mbox0 not empty
            sdio_cmd53_read(xfer_buf, 0x18001000 - 0x80, 1);
            return;
        }

        timeout--;
    }

    // Timeout
    return;
}

void sdio_send_mbox_block(uint8_t* xfer_buf, uint8_t* src) {
    uint32_t* tmp = (uint32_t*)xfer_buf;

    uint16_t len = ((uint16_t*)src)[2] + 6;
    if((uintptr_t)xfer_buf != (uintptr_t)src)
        memcpy(xfer_buf, src, len);
    
    xfer_buf += len;
    src += len;

    memset(xfer_buf, 0, (0u - len) & 0x7F);
    len += 0x7F;
    len &= ~0x7F;
    sdio_cmd53_write(tmp, 0x18001000 - len, len >> 7); // Length to 0x80 blocks
}

void sdio_bmi_wait_count4(void) {
    while(sdio_read_func1byte(0x450) == 0)
        ;
}

void sdio_bmi_1_done(void) {
    sdio_bmi_wait_count4();

    sdio_write_mbox0word(0x01); // BMI_DONE, Launches firmware
}

void sdio_bmi_3_write_memory(uint32_t* src, uint32_t dst, uint32_t len) {
    int32_t remaining = len;
    const uint32_t max_mbox_size = 0x200 - 0xC;

    uint32_t xfer_buf[0x200 / 4] = {0};

    while(remaining > 0) {
        uint32_t transfer_len = remaining;
        if(remaining > max_mbox_size)
            transfer_len = max_mbox_size;

        memcpy(xfer_buf + 0xC, src, transfer_len);
        sdio_bmi_wait_count4();

        xfer_buf[0] = 0x3; // BMI_WRITE_MEMORY
        xfer_buf[1] = dst;
        xfer_buf[2] = transfer_len;
        sdio_cmd53_write(xfer_buf, 0x10001000 - (transfer_len + 0xC), transfer_len + 0xC);

        src += transfer_len;
        dst += transfer_len;
        remaining -= transfer_len;
    }
}

uint32_t sdio_bmi_6_read_register(uint32_t addr) {
    sdio_bmi_wait_count4();

    uint32_t xfer_buf[0x200 / 4] = {0};
    xfer_buf[0] = 0x6; // BMI_READ_REGISTER
    xfer_buf[1] = addr;

    sdio_cmd53_write(xfer_buf, 0x10001000 - 0x8, 0x8);
    return sdio_read_mbox0word();
}

void sdio_bmi_7_write_register(uint32_t addr, uint32_t data) {
    sdio_bmi_wait_count4();

    uint32_t xfer_buf[0x200 / 4] = {0};
    xfer_buf[0] = 0x7; // BMI_WRITE_REGISTER
    xfer_buf[1] = addr;
    xfer_buf[2] = data;

    sdio_cmd53_write(xfer_buf, 0x10001000 - 0xC, 0xC);
}

uint32_t sdio_bmi_8_get_version(void) {
    sdio_bmi_wait_count4();
    sdio_write_mbox0word(0x8); // BMI_GET_VERSION
    uint32_t version = sdio_read_mbox0word();
    if(version == 0xffffffff) {
        uint32_t len = sdio_read_mbox0word() - 4; // Total length - 4

        if(len >= 0x80) {
            print("TODO: Nonlocal xfer_buf\n");
            return 0;
        }

        uint32_t xfer_buf[0x80] = {0};
        sdio_cmd53_read_mbox_to_xfer_buf(xfer_buf, len);

        version = xfer_buf[0]; // 1st = ROM version
    }

    return version;
}

uint32_t sdio_old_local_scratch0;
uint32_t sdio_old_wlan_system_sleep;

void sdio_bmi_init(void) {
    rom_version = sdio_bmi_8_get_version();

    uint32_t constant_2 = 0x2;
    sdio_bmi_3_write_memory(&constant_2, sdio_vars(), 4);
    sdio_old_local_scratch0 = sdio_bmi_6_read_register(0x180C0); // LOCAL_SCRATCH[0]
    sdio_bmi_7_write_register(0x180C0, sdio_old_local_scratch0 | 8); // LOCAL_SCRATCH[0]

    sdio_old_wlan_system_sleep = sdio_bmi_6_read_register(0x40C4); // WLAN_SYSTEM_SLEEP
    sdio_bmi_7_write_register(0x40C4, sdio_old_wlan_system_sleep | 1); // WLAN_SYSTEM_SLEEP

    sdio_bmi_7_write_register(0x4028, 0x5); // WLAN_CLOCK_CONTROL
    sdio_bmi_7_write_register(0x4020, 0); // WLAN_CPU_CLOCK
}

void sdio_bmi_finish(void) {
    sdio_bmi_7_write_register(0x40C4, sdio_old_wlan_system_sleep & ~1); // WLAN_SYSTEM_SLEEP
    sdio_bmi_7_write_register(0x180C0, sdio_old_local_scratch0);

    uint32_t constant_0x80 = 0x80;
    sdio_bmi_3_write_memory(&constant_0x80, sdio_vars() + 0x6C, 4); // HOST_RAM[0x6C] hi_mbox_io_block_sz

    uint32_t constant_0x63 = 0x63;
    sdio_bmi_3_write_memory(&constant_0x63, sdio_vars() + 0x74, 4); // HOST_RAM[0x74] hi_mbox_isr_yield_limit

    sdio_bmi_1_done(); // Launch Firmware
    while(sdio_read_intern_word(sdio_vars() + 0x58) != 1) // Wait until launched
        ;

    return;
}

uint32_t sdio_eeprom_addr;

void sdio_get_eeprom_stuff(void) {
    sdio_eeprom_addr = sdio_read_intern_word(sdio_vars() + 0x54); // HOST_RAM[0x54] hi_board_data
    sdio_read_intern_word(sdio_eeprom_addr); // EEPROM[0] = 0x300, size maybe
    sdio_read_intern_word(sdio_eeprom_addr + 0x10); // EEPROM[0x10], version maybe
}

void sdio_whatever_handshake(void) {
    uint32_t xfer_buf[0x200] = {0};

    // Named by launcher
    uint8_t handshake1[] = {0, 0, 0x8, 0, 0, 0, 2, 0, 0, 1, 0, 0, 0, 0}; // WMI_CONTROL ?
    uint8_t handshake2[] = {0, 0, 0x8, 0, 0, 0, 2, 0, 1, 1, 5, 0, 0, 0}; // WMI_DATA_BE best effort?
    uint8_t handshake3[] = {0, 0, 0x8, 0, 0, 0, 2, 0, 2, 1, 5, 0, 0, 0}; // WMI_DATA_BK background?
    uint8_t handshake4[] = {0, 0, 0x8, 0, 0, 0, 2, 0, 3, 1, 5, 0, 0, 0}; // WMI_DATA_VI video?
    uint8_t handshake5[] = {0, 0, 0x8, 0, 0, 0, 2, 0, 4, 1, 5, 0, 0, 0}; // WMI_DATA_VO voice?
    uint8_t handshake6[] = {0, 0, 0x2, 0, 0, 0, 4, 0}; // cmd_4 WMI_SYNCHRONIZE_CMD

    sdio_recv_mbox_block(xfer_buf); // HTC_MSG_READY_ID

    sdio_send_mbox_block((uint8_t*)xfer_buf, handshake1);
    sdio_recv_mbox_block(xfer_buf);

    sdio_send_mbox_block((uint8_t*)xfer_buf, handshake2);
    sdio_recv_mbox_block(xfer_buf);

    sdio_send_mbox_block((uint8_t*)xfer_buf, handshake3);
    sdio_recv_mbox_block(xfer_buf);

    sdio_send_mbox_block((uint8_t*)xfer_buf, handshake4);
    sdio_recv_mbox_block(xfer_buf);

    sdio_send_mbox_block((uint8_t*)xfer_buf, handshake5);
    sdio_recv_mbox_block(xfer_buf);

    sdio_send_mbox_block((uint8_t*)xfer_buf, handshake6);

    /* wifiboot does some weird assembly here? writing 1 doesn't work in any case
        ldr r0,=010300D1h
        mov r0, 1

        0x010300D1 does work, with 0x1 it freezes on the while, DSi Browser logs also use 0x010300D1
    */
    sdio_write_func1word(0x418, 0x010300D1); // INT_STATUS_ENABLE, mbox0notemptyIRQ
    sdio_write_func0byte(0x4, 0x3); // CCCR_INTERRUPT_ENABLE, enable master and func1 irqs

    while((sdio_read_func0byte(0x5) & 2) == 0)
        ;

    uint32_t app_ptr = sdio_read_intern_word(sdio_vars());
    sdio_write_intern_word(app_ptr, 2);
}

void sdio_atheros_init(void) {
    memcpy(twlcfg_etc_buf, (void*)0x2000400, 0x214); // Backup TWLCFGn.DAT area
    readFirmware(0x1FD, twlcfg_etc_buf + 0x1E0, 1);

    sdio_controller_init(); print("sdio_controller_init()\n");
    int need_upload = sdio_init_opcond_if_needed(); print("sdio_init_opcond_if_needed()\n");
    sdio_init_func0(); print("sdio_init_func0()\n");
    if(!need_upload) {
        need_upload = sdio_check_host_interest();
        print("sdio_check_host_interest()\n");
    }

    sdio_reset(); print("sdio_reset()\n");
    sdio_bmi_init(); print("sdio_bmi_init()\n");
    if(need_upload) {
        print("sdio: Need FW Uploading, bailing..\n");
        return;
    }
    sdio_bmi_finish(); print("sdio_bmi_finish()\n");

    sdio_get_eeprom_stuff(); print("sdio_get_eeprom_stuff()\n");
    sdio_whatever_handshake(); print("sdio_whatever_handshake()\n");

    char s[100] = {0};
    sprintf(s, "sdio: Chip ID: 0x%lx\n", chip_id);
    print(s);

    sprintf(s, "sdio: ROM Version: 0x%lx\n", rom_version);
    print(s);
}