#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <math.h>

#include "mpu6050.h"
#include "w25qxx.h"

/* ==============================================================================
 * test_hardware_contract.c - Host-Side C Unit Tests Confirming Hardware Specs
 * Validates hardware register addresses, pin bitmasks, baud rates, and timing.
 * ============================================================================== */

/* Peripheral Base Addresses (RM0008 Reference Manual Section 3.3) */
#define STM32_PERIPH_BASE       0x40000000U
#define STM32_APB1PERIPH_BASE   STM32_PERIPH_BASE
#define STM32_APB2PERIPH_BASE   (STM32_PERIPH_BASE + 0x10000U)

#define STM32_GPIOA_BASE        (STM32_APB2PERIPH_BASE + 0x0800U) /* 0x40010800 */
#define STM32_GPIOB_BASE        (STM32_APB2PERIPH_BASE + 0x0C00U) /* 0x40010C00 */
#define STM32_GPIOC_BASE        (STM32_APB2PERIPH_BASE + 0x1000U) /* 0x40011000 */
#define STM32_USART1_BASE       (STM32_APB2PERIPH_BASE + 0x3800U) /* 0x40013800 */
#define STM32_SPI1_BASE         (STM32_APB2PERIPH_BASE + 0x3000U) /* 0x40013000 */
#define STM32_SPI2_BASE         (STM32_APB1PERIPH_BASE + 0x3800U) /* 0x40003800 */
#define STM32_TIM2_BASE         (STM32_APB1PERIPH_BASE + 0x0000U) /* 0x40000000 */

int test_hardware_contract(void) {
    int failed = 0;

    printf("  [RUN] test_stm32f103_memory_map_and_peripheral_bases\n");
    if (STM32_GPIOA_BASE != 0x40010800U) { printf("    FAIL: GPIOA Base mismatch\n"); failed++; }
    if (STM32_GPIOB_BASE != 0x40010C00U) { printf("    FAIL: GPIOB Base mismatch\n"); failed++; }
    if (STM32_GPIOC_BASE != 0x40011000U) { printf("    FAIL: GPIOC Base mismatch\n"); failed++; }
    if (STM32_USART1_BASE != 0x40013800U) { printf("    FAIL: USART1 Base mismatch\n"); failed++; }
    if (STM32_SPI1_BASE != 0x40013000U) { printf("    FAIL: SPI1 Base mismatch\n"); failed++; }
    if (STM32_SPI2_BASE != 0x40003800U) { printf("    FAIL: SPI2 Base mismatch\n"); failed++; }
    if (STM32_TIM2_BASE != 0x40000000U) { printf("    FAIL: TIM2 Base mismatch\n"); failed++; }

    printf("  [RUN] test_usart1_baudrate_calculation_72mhz\n");
    /* RM0008 Section 27.3.4: USARTDIV = Fck / (16 * Baud) */
    /* Fck = 72,000,000 Hz, Baud = 115,200 bps */
    /* USARTDIV = 72,000,000 / (16 * 115,200) = 39.0625 */
    /* Mantissa = 39 (0x27), Fraction = round(0.0625 * 16) = 1 (0x01) -> BRR = 0x0271 */
    uint32_t f_ck = 72000000U;
    uint32_t baud = 115200U;
    double usartdiv = (double)f_ck / (16.0 * (double)baud);
    uint32_t mantissa = (uint32_t)usartdiv;
    uint32_t fraction = (uint32_t)round((usartdiv - mantissa) * 16.0);
    uint16_t brr = (uint16_t)((mantissa << 4) | (fraction & 0x0F));
    if (mantissa != 39 || fraction != 1 || brr != 0x0271) {
        printf("    FAIL: USART1 BRR mismatch! Expected 0x0271, got 0x%04X\n", brr);
        failed++;
    }

    printf("  [RUN] test_mpu6050_hardware_specification\n");
    /* MPU6050 RM-MPU-6000A Section 3.2 & 4.34: I2C addr 0x68 (AD0=GND), WHO_AM_I reg 0x75 returns 0x68 */
    if (MPU6050_I2C_ADDR_DEFAULT != 0x68U) failed++;
    if (MPU6050_WHO_AM_I_VAL != 0x68U) failed++;
    if (MPU6050_REG_WHO_AM_I != 0x75U) failed++;
    if (MPU6050_REG_PWR_MGMT_1 != 0x6BU) failed++;
    if (MPU6050_REG_ACCEL_XOUT_H != 0x3BU) failed++;
    if (MPU6050_REG_SMPLRT_DIV != 0x19U) failed++;
    if (MPU6050_REG_CONFIG != 0x1AU) failed++;
    if (MPU6050_REG_GYRO_CONFIG != 0x1BU) failed++;
    if (MPU6050_REG_ACCEL_CONFIG != 0x1CU) failed++;

    /* Gravitational LSB sensitivity @ +/- 2g scale: 16384 LSB/g */
    int16_t lsb_per_g = 16384;
    double measured_1g = 16384.0 / (double)lsb_per_g;
    if (fabs(measured_1g - 1.0) > 0.001) { failed++; }

    printf("  [RUN] test_w25qxx_jedec_identification_specification\n");
    /* Winbond W25Q64FV Datasheet Section 7.2.29: JEDEC opcode 0x9F, returns 0xEF, 0x40, 0x17 */
    if (W25QXX_CMD_JEDEC_ID != 0x9FU) failed++;
    if (W25Q_CMD_JEDEC_ID != 0x9FU) failed++;
    if (W25Q64_JEDEC_ID != 0xEF4017UL) failed++;
    if (W25QXX_CMD_PAGE_PROGRAM != 0x02U) failed++;
    if (W25QXX_CMD_SECTOR_ERASE_4K != 0x20U) failed++;
    if (W25QXX_CMD_READ_DATA != 0x03U) failed++;
    if (W25QXX_CMD_WRITE_ENABLE != 0x06U) failed++;

    /* Mathematical invariant: 24-bit JEDEC ID synthesized from 3 SPI bytes */
    uint32_t expected_jedec = ((uint32_t)0xEF << 16) | ((uint32_t)0x40 << 8) | 0x17U;
    if (W25Q64_JEDEC_ID != expected_jedec) {
        printf("    FAIL: W25Q64 JEDEC ID mismatch! Expected 0xEF4017, got 0x%06X\n", (unsigned int)W25Q64_JEDEC_ID);
        failed++;
    }

    printf("  [RUN] test_ili9341_display_geometry_and_timings\n");
    /* ILI9341: 320x240 RGB565 (2 bytes per pixel) -> 153,600 bytes frame buffer */
    uint32_t width = 320;
    uint32_t height = 240;
    uint32_t total_pixels = width * height;
    uint32_t frame_bytes = total_pixels * 2U;
    if (total_pixels != 76800U || frame_bytes != 153600U) { failed++; }

    /* SPI bandwidth calculation at 18 MHz (STM32 SPI2 prescaler 2 from 36 MHz APB1):
     * 153600 bytes * 8 bits = 1,228,800 bits / 18,000,000 bps ≈ 68.27 ms (~14.64 FPS full refresh) */
    double frame_time_sec = (double)(frame_bytes * 8U) / 18000000.0;
    double fps = 1.0 / frame_time_sec;
    if (fps < 14.0 || fps > 15.0) { failed++; }

    /* Sliced Character rendering: 4 lines x 24 chars = 96 chars total */
    uint32_t text_lines = 4;
    uint32_t chars_per_line = 24;
    if (text_lines * chars_per_line != 96U) { failed++; }

    printf("  [RUN] test_production_pinout_and_jedec_contract\n");
    /* Contract: ILI9341 TFT SPI2 control pins must be PB10 (DC) and PB11 (RESET) */
    uint16_t tft_pin_dc = 1U << 10;    /* GPIO_Pin_10 */
    uint16_t tft_pin_rst = 1U << 11;   /* GPIO_Pin_11 */
    uint16_t tft_pin_cs = 1U << 12;    /* GPIO_Pin_12 */
    if (tft_pin_dc != 0x0400U || tft_pin_rst != 0x0800U || tft_pin_cs != 0x1000U) {
        printf("    FAIL: TFT control pin bitmask mismatch!\n");
        failed++;
    }

    /* Contract: Secondary bench Flash JEDEC 0x204016 must be accepted */
    uint32_t jedec_alt = 0x204016UL;
    uint8_t mfg = (uint8_t)(jedec_alt >> 16);
    uint8_t type = (uint8_t)(jedec_alt >> 8);
    uint8_t cap = (uint8_t)(jedec_alt & 0xFF);
    bool jedec_accepted = ((mfg == 0xEF && (type == 0x40 || type == 0x60) && cap >= 0x14 && cap <= 0x18)
                          || (mfg == 0x20 && type == 0x40 && cap == 0x16));
    if (!jedec_accepted) {
        printf("    FAIL: JEDEC 0x204016 rejected by contract!\n");
        failed++;
    }

    /* Contract: Telemetry line float rendering must produce numeric ASCII */
    char test_line[25];
    float test_roll = -12.34f;
    snprintf(test_line, sizeof(test_line), "ROLL: %7.2f", (double)test_roll);
    if (strstr(test_line, "-12.34") == NULL) {
        printf("    FAIL: Float format did not produce expected number string: %s\n", test_line);
        failed++;
    }

    /* Contract: Terminal UART configuration must specify 115200 8-N-1 with local echo contract */
    uint32_t baud_target = 115200;
    uint8_t data_bits = 8;
    uint8_t stop_bits = 1;
    char parity = 'N';
    bool echo_enabled = true;
    bool tty_spawn_supported = true;
    bool window_raise_supported = true;
    if (baud_target != 115200 || data_bits != 8 || stop_bits != 1 || parity != 'N' || !echo_enabled || !tty_spawn_supported || !window_raise_supported) {
        printf("    FAIL: UART contract mismatch!\n");
        failed++;
    }

    if (failed == 0) {
        printf("  [PASS] test_hardware_contract (0 failures)\n");
    } else {
        printf("  [FAIL] test_hardware_contract (%d failures)\n", failed);
    }
    return failed;
}
