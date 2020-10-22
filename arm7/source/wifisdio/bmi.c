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
    const uint32_t max_mbox_size = 0x200 - 0xC;

    uint32_t xfer_buf[0x200 / 4] = {0};

    while(remaining > 0) {
        uint32_t transfer_len = remaining;
        if(remaining > max_mbox_size)
            transfer_len = max_mbox_size;

        memcpy(xfer_buf + 0xC, src, transfer_len);
        sdio_bmi_wait_count4();

        xfer_buf[0] = BMI_WRITE_MEMORY;
        xfer_buf[1] = dst;
        xfer_buf[2] = transfer_len;
        sdio_cmd53_write(xfer_buf, 0x10001000 - (transfer_len + 0xC), transfer_len + 0xC);

        src += transfer_len;
        dst += transfer_len;
        remaining -= transfer_len;
    }
}

uint32_t sdio_bmi_read_soc_register(uint32_t addr) {
    sdio_bmi_wait_count4();

    uint32_t xfer_buf[0x200 / 4] = {0};
    xfer_buf[0] = BMI_READ_SOC_REGISTER;
    xfer_buf[1] = addr;

    sdio_cmd53_write(xfer_buf, 0x10001000 - 0x8, 0x8);
    return sdio_read_mbox_word(0);
}

void sdio_bmi_write_soc_register(uint32_t addr, uint32_t data) {
    sdio_bmi_wait_count4();

    uint32_t xfer_buf[0x200 / 4] = {0};
    xfer_buf[0] = BMI_WRITE_SOC_REGISTER;
    xfer_buf[1] = addr;
    xfer_buf[2] = data;

    sdio_cmd53_write(xfer_buf, 0x10001000 - 0xC, 0xC);
}

uint32_t sdio_bmi_get_version(void) {
    sdio_bmi_wait_count4();
    sdio_write_mbox_word(0, BMI_GET_TARGET_ID);
    uint32_t version = sdio_read_mbox_word(0);
    if(version == 0xffffffff) {
        uint32_t len = sdio_read_mbox_word(0) - 4; // Total length - 4

        if(len >= 0x80) {
            print("TODO: Nonlocal xfer_buf\n");
            return 0;
        }

        uint32_t xfer_buf[0x80] = {0};
        sdio_cmd53_read_mbox_to_xfer_buf(0, xfer_buf, len);

        version = xfer_buf[0]; // 1st = ROM version
    }

    return version;
}