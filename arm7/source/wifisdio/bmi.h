#pragma once

#include <stdint.h>
#include <stddef.h>

#define BMI_NOP (0x0) // No-op
#define BMI_DONE (0x1) // Exit BMI mode, start WMI commands
#define BMI_READ_MEMORY (0x2) // Read Xtensa RAM in byte units
#define BMI_WRITE_MEMORY (0x3) // Write Xtensa RAM in byte units
#define BMI_EXECUTE (0x4) // Execute a function on the Xtensa CPU
#define BMI_SET_APP_START (0x5) // Change the entrypoint for BMI_DONE
#define BMI_READ_SOC_REGISTER (0x6) // Read Xtensa RAM in a 32bit unit - used for reading I/O ports
#define BMI_WRITE_SOC_REGISTER (0x7) // Write Xtensa RAM in a 32bit unit - used for writing I/O ports
#define BMI_GET_TARGET_ID (0x8) // Get ROM Version
#define BMI_ROMPATCH_INSTALL (0x9) // Install a ROM Patch
#define BMI_ROMPATCH_UNINSTALL (0xA) // Uninstall a ROM Patch
#define BMI_ROMPATCH_ACTIVATE (0xB) // Activate a ROM Patch
#define BMI_ROMPATCH_DEACTIVATE (0xC) // Deactivate a ROM Patch
#define BMI_LZ_STREAM_START (0xD) // Set destination for folloing BMI_LZ_DATA commands
#define BMI_LZ_DATA (0xE) // Send LZ compressed data
#define BMI_NVRAM_PROCESS (0xF) // Unimplemented in DSi, ignored

void sdio_bmi_done(void); // Exits BMI mode
void sdio_bmi_write_memory(uint32_t* src, uint32_t dst, size_t len); // Writes to Xtensa memory
uint32_t sdio_bmi_read_soc_register(uint32_t addr); // Reads a 32bit I/O register
void sdio_bmi_write_soc_register(uint32_t addr, uint32_t data); // Writes a 32bit I/O register
uint32_t sdio_bmi_get_version(void); // Gets ROM version