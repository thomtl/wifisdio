#include "wifisdio.h"
#include "sdio.h"
#include "bmi.h"
#include "wmi.h"
#include "wifi.h"

#include "ndma.h"

#include <string.h>
#include <stdio.h>

// Globals
uint32_t chip_id, rom_version, regulatory_domain, regulatory_channels = 0;
//uint8_t twlcfg_etc_buf[0x214] = {0};
uint32_t sdio_xfer_buf[0xA00 + 14 + 2];


uint16_t current_channel = 0;
sgWifiAp_t access_points[2];

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
    uint32_t irq_stat = SDIO_CARD_IRQ_STAT; // Read to ACK
    (void)irq_stat;
    SDIO_CARD_IRQ_MASK &= ~1; // Unmask IRQ
    SDIO_CARD_IRQ_CTL |= 1; // Enable IRQ
    #endif

    GPIO_WIFI &= ~(1 << 8); // Tell HW we're using DSiWifi and not NDSWifi

    i2cWriteRegister(I2C_PM, 0x30, 0x13); // Enable WIFISDIO + WiFi LED blink on transfer
}

int sdio_init_opcond_if_needed(void) {
    uint8_t irq_flags = sdio_read_func_byte(0, FUNC0_CCCR_IRQ_FLAGS);
    (void)irq_flags;

    // If it passed okay, skip cmd5, etc
    if((SDIO_IRQ_STAT & 0x400000) == 0)
        return 0; // Need Upload

    uint32_t known_voltages = 0;
    while(true) {
        known_voltages &= 0x100000; // bit20: 3.2V..3.3V

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
    //sdio_write_func_byte(0, FUNC0_CCCR_IRQ_FLAGS, 0); // Disable all IRQs, wifiboot does not leave dsiwifi in a compatible state to unlaunch, so we have to explicitly disable these
    sdio_write_func_byte(0, FUNC0_CCCR_POWER_CONTROL, 0x02); // CCCR Power Control
    sdio_write_func_byte(0, FUNC0_CCCR_BUS_INTERFACE, 0x82); // CCCR Bus Interface
    sdio_write_func_byte(0, FUNC0_CCCR_CARD_CAPS, 0x17); // CCCR Card Capabilities
    sdio_write_func_byte(0, FUNC0_FBRn_BLOCK_SIZE_LOW(1), 0x80); // FBR1 Block Size lsb
    sdio_write_func_byte(0, FUNC0_FBRn_BLOCK_SIZE_HIGH(1), 0x00); // FBR1 Block Size msb
    sdio_write_func_byte(0, FUNC0_CCCR_FUNCTION, 0x02); // Function Enable

    while(sdio_read_func_byte(0, FUNC0_CCCR_FUNCTION + 1) != 0x2) // Function Ready
        ;

    sdio_write_func_word(1, FUNC1_INT_STATUS_ENABLE, 0); // Disable all FUNC1 IRQs
    chip_id = sdio_read_intern_word(0x000040ec); // Read CHIP_ID from Xtensa RAM
}

// Aka check if FW is already uploaded
bool sdio_check_host_interest(void) {
    // See sdio_vars for why this is commented out
    /*uint16_t crc16 = swiCRC16(0xFFFF, twlcfg_etc_buf + 0x1E4, 0xC);
    if(crc16 != *((uint16_t*)(twlcfg_etc_buf + 0x1E2)))
        return true; // Need upload*/

    uint32_t uploaded = sdio_read_intern_word(sdio_vars() + 0x58);
    return (uploaded == 1) ? 0 : 1;
}

void sdio_reset(void) {
    sdio_write_intern_word(0x4000, 0x100); // Reset control, bit 8 = reset
    swiDelay(0x10000); // Delay is needed
    sdio_read_intern_word(0x40C0); // Reset cause
}

uint32_t sdio_old_local_scratch0;
uint32_t sdio_old_wlan_system_sleep;

void sdio_bmi_init(void) {
    rom_version = sdio_bmi_get_version();

    uint32_t constant_2 = 0x2;
    sdio_bmi_write_memory(&constant_2, sdio_vars(), 4);
    sdio_old_local_scratch0 = sdio_bmi_read_soc_register(0x180C0); // LOCAL_SCRATCH[0]
    sdio_bmi_write_soc_register(0x180C0, sdio_old_local_scratch0 | 8); // LOCAL_SCRATCH[0]

    sdio_old_wlan_system_sleep = sdio_bmi_read_soc_register(0x40C4); // WLAN_SYSTEM_SLEEP
    sdio_bmi_write_soc_register(0x40C4, sdio_old_wlan_system_sleep | 1); // WLAN_SYSTEM_SLEEP

    sdio_bmi_write_soc_register(0x4028, 0x5); // WLAN_CLOCK_CONTROL
    sdio_bmi_write_soc_register(0x4020, 0); // WLAN_CPU_CLOCK
}

void sdio_bmi_finish(void) {
    sdio_bmi_write_soc_register(0x40C4, sdio_old_wlan_system_sleep & ~1); // WLAN_SYSTEM_SLEEP
    sdio_bmi_write_soc_register(0x180C0, sdio_old_local_scratch0); // LOCAL_SCRATCH[0]

    uint32_t constant_0x80 = 0x80;
    sdio_bmi_write_memory(&constant_0x80, sdio_vars() + 0x6C, 4); // HOST_RAM[0x6C] hi_mbox_io_block_sz

    uint32_t constant_0x63 = 0x63;
    sdio_bmi_write_memory(&constant_0x63, sdio_vars() + 0x74, 4); // HOST_RAM[0x74] hi_mbox_isr_yield_limit

    sdio_bmi_done(); // Launch Firmware
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

// Maybe called BootInfo Handshake, Or HTC_SERVICES handshake
void sdio_whatever_handshake(void) {
    // Named such by launcher
    uint8_t handshake1[] = {0, 0, 0x8, 0, 0, 0, 2, 0, 0, 1, 0, 0, 0, 0}; // WMI_CONTROL ?
    uint8_t handshake2[] = {0, 0, 0x8, 0, 0, 0, 2, 0, 1, 1, 5, 0, 0, 0}; // WMI_DATA_BE best effort
    uint8_t handshake3[] = {0, 0, 0x8, 0, 0, 0, 2, 0, 2, 1, 5, 0, 0, 0}; // WMI_DATA_BK background
    uint8_t handshake4[] = {0, 0, 0x8, 0, 0, 0, 2, 0, 3, 1, 5, 0, 0, 0}; // WMI_DATA_VI video
    uint8_t handshake5[] = {0, 0, 0x8, 0, 0, 0, 2, 0, 4, 1, 5, 0, 0, 0}; // WMI_DATA_VO voice
    uint8_t handshake6[] = {0, 0, 0x2, 0, 0, 0, 4, 0}; // cmd_4 WMI_SYNCHRONIZE_CMD

    sdio_recv_mbox_block(0); // HTC_MSG_READY_ID
    sdio_send_mbox_block(0, handshake1);
    
    sdio_recv_mbox_block(0);
    sdio_send_mbox_block(0, handshake2);

    sdio_recv_mbox_block(0);
    sdio_send_mbox_block(0, handshake3);

    sdio_recv_mbox_block(0);
    sdio_send_mbox_block(0, handshake4);

    sdio_recv_mbox_block(0);
    sdio_send_mbox_block(0, handshake5);

    sdio_recv_mbox_block(0);
    sdio_send_mbox_block(0, handshake6);

    /* wifiboot does some weird assembly here? writing 1 doesn't work in any case
        ldr r0,=010300D1h
        mov r0, 1

        0x010300D1 does work, with 0x1 it freezes on the while, DSi Browser logs also use 0x010300D1
    */
    sdio_write_func_word(1, FUNC1_INT_STATUS_ENABLE, 0x010300D1); // Enable MBOX0 Not Empty IRQ
    sdio_write_func_byte(0, FUNC0_CCCR_IRQ_FLAGS, 0x3); // Enable Master and FUNC1 irqs

    while((sdio_read_func_byte(0, 0x5) & 2) == 0)
        ;

    uint32_t app_ptr = sdio_read_intern_word(sdio_vars());
    sdio_write_intern_word(app_ptr, 2);
}

void sdio_atheros_init(void) {
    //memcpy(twlcfg_etc_buf, (void*)0x2000400, 0x214); // Backup TWLCFGn.DAT area
    //readFirmware(0x1FD, twlcfg_etc_buf + 0x1E0, 1);

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

    print("sdio: Chip ID: 0x%lx\n", chip_id);
    print("sdio: ROM Version: 0x%lx\n", rom_version);
}

void sdio_prepare_scanning(void) {
    sdio_wmi_error_report_cmd(0, 0x7F); print("sdio_wmi_error_report()\n");
    sdio_wmi_start_whatever_timer_cmd(0, 2); print("sdio_wmi_start_whatever_timer()\n");
    sdio_wmi_get_channel_list_cmd(0); print("sdio_wmi_get_channel_list()\n");
    sdio_poll_mbox(0); print("sdio_poll_mbox()\n");
}

void Wifi_Init_Core(void) {
    // TODO: Load actual AP data and shite
    extern uint32_t boot_channel_wait;

    boot_channel_wait = 2;

    extern uint16_t requested_channel;
    extern uint8_t boot_channel_list[];
    requested_channel = boot_channel_list[0];
    current_channel  = requested_channel;
}

void sdio_init(void) {
    //powerOn(1 << 1); // Power up DSWIFI, is this really needed?
    
    NDMAGCNT = 0; // Use linear priority mode

    Wifi_Init_Core();

    sdio_atheros_init();
    sdio_prepare_scanning();

    sdio_wmi_scan_channel();
    sdio_wmi_scan_channel();
    sdio_wmi_scan_channel();
}