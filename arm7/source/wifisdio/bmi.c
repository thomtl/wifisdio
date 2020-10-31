#include "wifisdio.h"
#include "bmi.h"

#include "sdio.h"

#include <string.h>

static void sdio_bmi_wait_count4(void) {
    while(sdio_read_func_byte(1, 0x450) == 0)
        ;
}

void sdio_bmi_done(void) {
    sdio_bmi_wait_count4();

    sdio_write_mbox_word(0, BMI_DONE);
}

void sdio_bmi_write_memory(uint32_t* src, uint32_t dst, size_t len) {
    int32_t remaining = len;
    const int32_t max_mbox_size = 0x200 - 0xC;

    uint8_t* data = (uint8_t*)src;

    while(remaining > 0) {
        uint32_t transfer_len = remaining;
        if(remaining > max_mbox_size)
            transfer_len = max_mbox_size;

        memcpy(sdio_xfer_buf + (0xC / 4), data, transfer_len);
        sdio_bmi_wait_count4();

        sdio_xfer_buf[0] = BMI_WRITE_MEMORY;
        sdio_xfer_buf[1] = dst;
        sdio_xfer_buf[2] = transfer_len;
        sdio_cmd53_write(sdio_xfer_buf, 0x10001000 - (transfer_len + 0xC), transfer_len + 0xC);

        data += transfer_len;
        dst += transfer_len;
        remaining -= transfer_len;
    }
}

uint32_t sdio_bmi_read_soc_register(uint32_t addr) {
    sdio_bmi_wait_count4();

    sdio_xfer_buf[0] = BMI_READ_SOC_REGISTER;
    sdio_xfer_buf[1] = addr;

    sdio_cmd53_write(sdio_xfer_buf, 0x10001000 - 0x8, 0x8);
    return sdio_read_mbox_word(0);
}

void sdio_bmi_write_soc_register(uint32_t addr, uint32_t data) {
    sdio_bmi_wait_count4();

    sdio_xfer_buf[0] = BMI_WRITE_SOC_REGISTER;
    sdio_xfer_buf[1] = addr;
    sdio_xfer_buf[2] = data;

    sdio_cmd53_write(sdio_xfer_buf, 0x10001000 - 0xC, 0xC);
}

uint32_t sdio_bmi_get_version(void) {
    sdio_bmi_wait_count4();
    sdio_write_mbox_word(0, BMI_GET_TARGET_ID);
    uint32_t version = sdio_read_mbox_word(0);
    if(version == 0xffffffff) {
        uint32_t len = sdio_read_mbox_word(0) - 4; // Total length - 4

        sdio_cmd53_read_mbox_to_xfer_buf(0, len);

        version = sdio_xfer_buf[0]; // 1st = ROM version
    }

    return version;
}