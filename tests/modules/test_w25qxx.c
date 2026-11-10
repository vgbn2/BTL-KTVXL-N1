#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <math.h>
#include "w25qxx.h"
#include "storage_format.h"

int test_w25qxx_module(void) {
    int failed = 0;

    printf("  [RUN] test_w25qxx_constants\n");
    if (W25QXX_CMD_WRITE_ENABLE != 0x06) failed++;
    if (W25QXX_CMD_WRITE_DISABLE != 0x04) failed++;
    if (W25QXX_CMD_READ_STATUS_R1 != 0x05) failed++;
    if (W25QXX_CMD_READ_DATA != 0x03) failed++;
    if (W25QXX_CMD_PAGE_PROGRAM != 0x02) failed++;
    if (W25QXX_CMD_SECTOR_ERASE_4K != 0x20) failed++;
    if (W25QXX_CMD_JEDEC_ID != 0x9F) failed++;
    if (W25Q64_JEDEC_ID != 0xEF4017) failed++;

    printf("  [RUN] test_w25qxx_init\n");
    w25qxx_set_mock_jedec_id(0xEF, 0x40, 0x17);
    bool ok = w25qxx_init();
    if (!ok) failed++;

    uint32_t jedec = w25qxx_read_jedec_id();
    if (jedec != W25Q64_JEDEC_ID) failed++;
    if ((jedec >> 16) != 0xEF) failed++;         /* Winbond Manufacturer ID */
    if (((jedec >> 8) & 0xFF) != 0x40) failed++; /* SPI Memory Type */
    if ((jedec & 0xFF) != 0x17) failed++;        /* 64M-Bit Capacity */

    /* Fault injection: disconnected bus (all 0xFF) and non-Winbond (0x00) rejection */
    w25qxx_set_mock_jedec_id(0xFF, 0xFF, 0xFF);
    if (w25qxx_init() != false) failed++;

    w25qxx_set_mock_jedec_id(0x00, 0x00, 0x00);
    if (w25qxx_init() != false) failed++;

    /* Restore genuine Winbond ID */
    w25qxx_set_mock_jedec_id(0xEF, 0x40, 0x17);
    if (!w25qxx_init()) failed++;

    printf("  [RUN] test_w25qxx_erase_and_raw_rw\n");
    uint8_t write_buf[16] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
                             0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88};
    uint8_t read_buf[16];
    memset(read_buf, 0, sizeof(read_buf));

    /* Sector 1: offset 4096 */
    uint32_t test_addr = 4096;
    ok = w25qxx_erase_sector(test_addr);
    if (!ok) failed++;

    /* Verify erased to 0xFF */
    ok = w25qxx_read_data(test_addr, read_buf, sizeof(read_buf));
    if (!ok) failed++;
    for (size_t i = 0; i < sizeof(read_buf); i++) {
        if (read_buf[i] != 0xFF) {
            failed++;
            break;
        }
    }

    /* Program page data */
    ok = w25qxx_write_page(test_addr, write_buf, sizeof(write_buf));
    if (!ok) failed++;

    /* Read back and verify */
    ok = w25qxx_read_data(test_addr, read_buf, sizeof(read_buf));
    if (!ok) failed++;
    if (memcmp(write_buf, read_buf, sizeof(write_buf)) != 0) failed++;

    /* NOR Flash physical property test: programming cannot set 0 back to 1 without erase */
    uint8_t zero_byte = 0x00;
    uint8_t ff_byte = 0xFF;
    w25qxx_write_page(test_addr, &zero_byte, 1);
    w25qxx_read_data(test_addr, read_buf, 1);
    if (read_buf[0] != 0x00) failed++;

    /* Attempt to write 0xFF over 0x00 without erase -> bitwise AND preserves 0 */
    w25qxx_write_page(test_addr, &ff_byte, 1);
    w25qxx_read_data(test_addr, read_buf, 1);
    if (read_buf[0] != 0x00) failed++;

    /* Clean up sector with erase */
    w25qxx_erase_sector(test_addr);

    printf("  [RUN] test_w25qxx_config_save_load\n");
    flash_config_block_t save_cfg;
    memset(&save_cfg, 0, sizeof(save_cfg));
    save_cfg.alpha = 0.96f;
    save_cfg.accel_offset_x = 320;
    save_cfg.accel_offset_y = -180;
    save_cfg.accel_offset_z = 15;
    save_cfg.gyro_offset_x = 7;
    save_cfg.gyro_offset_y = -3;
    save_cfg.gyro_offset_z = 11;
    save_cfg.write_count = 5;

    ok = w25qxx_save_config(&save_cfg);
    if (!ok) failed++;

    flash_config_block_t loaded_cfg;
    memset(&loaded_cfg, 0, sizeof(loaded_cfg));
    ok = w25qxx_load_config(&loaded_cfg);
    if (!ok) failed++;
    if (loaded_cfg.magic != FLASH_CONFIG_MAGIC) failed++;
    if (loaded_cfg.version != FLASH_CONFIG_VERSION) failed++;
    if (fabsf(loaded_cfg.alpha - 0.96f) >= 1e-4) failed++;
    if (loaded_cfg.accel_offset_x != 320) failed++;
    if (loaded_cfg.accel_offset_y != -180) failed++;
    if (loaded_cfg.accel_offset_z != 15) failed++;
    if (loaded_cfg.gyro_offset_x != 7) failed++;
    if (loaded_cfg.gyro_offset_y != -3) failed++;
    if (loaded_cfg.gyro_offset_z != 11) failed++;
    if (loaded_cfg.write_count != 5) failed++;
    if (storage_validate_config(&loaded_cfg) != true) failed++;

    if (failed == 0) {
        printf("  [PASS] test_w25qxx_module (0 failures)\n");
    } else {
        fprintf(stderr, "  [FAIL] test_w25qxx_module (%d failures)\n", failed);
    }
    return failed;
}
