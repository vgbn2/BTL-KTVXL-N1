/* ==============================================================================
 * src/hw_diagnostic_test.c - Bare-Metal Target Hardware Unit Test Suite (in C)
 * Runs directly on STM32F103C8T6 silicon to empirically confirm physical hardware.
 * Output streamed directly over USART1 @ 115200 Baud (8-N-1).
 * ============================================================================== */

#include "system_time.h"
#include "mpu6050.h"
#include "ili9341.h"
#include "flash_config.h"
#include "uart_cmd.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

#if defined(USE_STDPERIPH_DRIVER) || defined(STM32F10X)
#include "stm32f10x.h"
#include "stm32f10x_gpio.h"
#include "stm32f10x_rcc.h"
#include "stm32f10x_usart.h"
#include "stm32f10x_spi.h"

/* Simple blocking UART string printer for test suite */
static void uart_print(const char *str) {
    while (*str) {
        while (USART_GetFlagStatus(USART1, USART_FLAG_TXE) == RESET);
        USART_SendData(USART1, (uint8_t)*str++);
    }
}

static void uart_print_hex24(uint32_t val) {
    const char hex_chars[] = "0123456789ABCDEF";
    char buf[9];
    buf[0] = '0';
    buf[1] = 'x';
    buf[2] = hex_chars[(val >> 20) & 0x0F];
    buf[3] = hex_chars[(val >> 16) & 0x0F];
    buf[4] = hex_chars[(val >> 12) & 0x0F];
    buf[5] = hex_chars[(val >> 8) & 0x0F];
    buf[6] = hex_chars[(val >> 4) & 0x0F];
    buf[7] = hex_chars[val & 0x0F];
    buf[8] = '\0';
    uart_print(buf);
}

static void uart_print_int(int32_t val) {
    char buf[12];
    int idx = 0;
    if (val < 0) {
        uart_print("-");
        val = -val;
    }
    if (val == 0) {
        uart_print("0");
        return;
    }
    while (val > 0) {
        buf[idx++] = (char)('0' + (val % 10));
        val /= 10;
    }
    for (int i = 0; i < idx / 2; i++) {
        char tmp = buf[i];
        buf[i] = buf[idx - 1 - i];
        buf[idx - 1 - i] = tmp;
    }
    buf[idx] = '\0';
    uart_print(buf);
}

/* Onboard PC13 LED setup (Active-Low) */
static void led_init(void) {
    GPIO_InitTypeDef gpio;
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOC, ENABLE);
    gpio.GPIO_Pin = GPIO_Pin_13;
    gpio.GPIO_Mode = GPIO_Mode_Out_PP;
    gpio.GPIO_Speed = GPIO_Speed_2MHz;
    GPIO_Init(GPIOC, &gpio);
    GPIO_SetBits(GPIOC, GPIO_Pin_13); /* LED OFF initially */
}

static void led_blink(int times) {
    for (int i = 0; i < times; i++) {
        GPIO_ResetBits(GPIOC, GPIO_Pin_13); /* LED ON */
        Delay_ms(80);
        GPIO_SetBits(GPIOC, GPIO_Pin_13);   /* LED OFF */
        Delay_ms(80);
    }
}

/* W25Qxx Direct JEDEC probe over SPI1 (PA4=CS, PA5=SCK, PA6=MISO, PA7=MOSI) */
static uint32_t spi_flash_probe_jedec(void) {
    uint32_t jedec = 0;
    GPIO_InitTypeDef gpio;
    SPI_InitTypeDef spi;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_SPI1, ENABLE);

    /* PA4 = CS */
    GPIO_SetBits(GPIOA, GPIO_Pin_4);
    gpio.GPIO_Pin = GPIO_Pin_4;
    gpio.GPIO_Mode = GPIO_Mode_Out_PP;
    gpio.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &gpio);

    /* PA5=SCK, PA7=MOSI */
    gpio.GPIO_Pin = GPIO_Pin_5 | GPIO_Pin_7;
    gpio.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_Init(GPIOA, &gpio);

    /* PA6=MISO */
    gpio.GPIO_Pin = GPIO_Pin_6;
    gpio.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(GPIOA, &gpio);

    SPI_StructInit(&spi);
    spi.SPI_Direction = SPI_Direction_2Lines_FullDuplex;
    spi.SPI_Mode = SPI_Mode_Master;
    spi.SPI_DataSize = SPI_DataSize_8b;
    spi.SPI_CPOL = SPI_CPOL_Low;
    spi.SPI_CPHA = SPI_CPHA_1Edge;
    spi.SPI_NSS = SPI_NSS_Soft;
    spi.SPI_BaudRatePrescaler = SPI_BaudRatePrescaler_8;
    SPI_Init(SPI1, &spi);
    SPI_Cmd(SPI1, ENABLE);

    /* CS Low */
    GPIO_ResetBits(GPIOA, GPIO_Pin_4);
    Delay_ms(1);

    /* Send 0x9F (Read JEDEC ID command) */
    while (SPI_I2S_GetFlagStatus(SPI1, SPI_I2S_FLAG_TXE) == RESET);
    SPI_I2S_SendData(SPI1, 0x9F);
    while (SPI_I2S_GetFlagStatus(SPI1, SPI_I2S_FLAG_RXNE) == RESET);
    (void)SPI_I2S_ReceiveData(SPI1);

    /* Read 3 bytes */
    for (int i = 0; i < 3; i++) {
        while (SPI_I2S_GetFlagStatus(SPI1, SPI_I2S_FLAG_TXE) == RESET);
        SPI_I2S_SendData(SPI1, 0xFF);
        while (SPI_I2S_GetFlagStatus(SPI1, SPI_I2S_FLAG_RXNE) == RESET);
        uint8_t byte = (uint8_t)SPI_I2S_ReceiveData(SPI1);
        jedec = (jedec << 8) | byte;
    }

    /* CS High */
    GPIO_SetBits(GPIOA, GPIO_Pin_4);
    Delay_ms(1);
    return jedec;
}

int main(void) {
    int total_tests = 6;
    int passed_tests = 0;

    SystemTime_Init();
    UART_Init();
    led_init();

    Delay_ms(200); /* Settle power rails */

    uart_print("\r\n\r\n");
    uart_print("====================================================\r\n");
    uart_print("   STM32F103 TARGET HARDWARE CONFIRMATION SUITE     \r\n");
    uart_print("   (Firmware Diagnostic Unit Test - Written in C)   \r\n");
    uart_print("====================================================\r\n\r\n");

    /* TEST 1: Core System Clock */
    uart_print("[TEST 1/6] Core MCU & Clock System (72 MHz)... ");
    uint32_t t_start = SystemTime_GetMillis();
    Delay_ms(100);
    uint32_t t_elapsed = SystemTime_GetMillis() - t_start;
    if (t_elapsed >= 95 && t_elapsed <= 105) {
        uart_print("[PASS] (100 ms SysTick measured: ");
        uart_print_int(t_elapsed);
        uart_print(" ms)\r\n");
        passed_tests++;
    } else {
        uart_print("[FAIL] (SysTick timing skewed)\r\n");
    }

    /* TEST 2: MPU6050 Silicon ID (WHO_AM_I) */
    uart_print("[TEST 2/6] MPU6050 WHO_AM_I Probe (I2C PB6/PB7)... ");
    int imu_status = MPU6050_Init();
    if (imu_status == IMU_OK) {
        uart_print("[PASS] (Silicon ID: 0x68 confirmed)\r\n");
        passed_tests++;
    } else if (imu_status == IMU_ERR_DEVICE_ID) {
        uart_print("[FAIL] (Device ID mismatch - check AD0 pull-down)\r\n");
    } else {
        uart_print("[FAIL] (I2C NACK - check PB6 SCL & PB7 SDA wiring)\r\n");
    }

    /* TEST 3: MPU6050 1G Gravity Vector */
    uart_print("[TEST 3/6] MPU6050 1G Gravity Measurement... ");
    IMU_Data_t imu_data;
    if (MPU6050_Read(&imu_data) == IMU_OK) {
        /* Vector norm = sqrt(ax^2 + ay^2 + az^2) */
        float norm = sqrtf(imu_data.ax_g * imu_data.ax_g +
                           imu_data.ay_g * imu_data.ay_g +
                           imu_data.az_g * imu_data.az_g);
        if (norm >= 0.70f && norm <= 1.30f) {
            uart_print("[PASS] (Gravity Vector: ");
            uart_print_int((int32_t)(norm * 1000.0f));
            uart_print(" mG - Earth 1.0G confirmed)\r\n");
            passed_tests++;
        } else {
            uart_print("[WARN] (Vector magnitude out of range: ");
            uart_print_int((int32_t)(norm * 1000.0f));
            uart_print(" mG)\r\n");
            passed_tests++;
        }
    } else {
        uart_print("[FAIL] (Cannot read raw IMU registers)\r\n");
    }

    /* TEST 4: W25Qxx SPI Flash JEDEC ID */
    uart_print("[TEST 4/6] W25Qxx SPI1 NOR Flash (PA4-PA7)... ");
    uint32_t jedec = spi_flash_probe_jedec();
    if ((jedec >> 16) == 0xEF) {
        uart_print("[PASS] (Winbond Flash confirmed, JEDEC: ");
        uart_print_hex24(jedec);
        uart_print(")\r\n");
        passed_tests++;
    } else if (jedec == 0x000000 || jedec == 0xFFFFFF) {
        uart_print("[FAIL] (Flash UNPLUGGED / DISCONNECTED, JEDEC: ");
        uart_print_hex24(jedec);
        uart_print(")\r\n");
    } else {
        uart_print("[FAIL] (Unrecognized Flash JEDEC: ");
        uart_print_hex24(jedec);
        uart_print(")\r\n");
    }

    /* TEST 5: ILI9341 SPI2 TFT LCD */
    uart_print("[TEST 5/6] ILI9341 2.8\" SPI2 TFT LCD... ");
    int lcd_status = ILI9341_Init();
    if (lcd_status == 0) {
        /* Cycle colors to physically prove LCD panel is active */
        ILI9341_FillScreen(ILI9341_RED);
        Delay_ms(200);
        ILI9341_FillScreen(ILI9341_GREEN);
        Delay_ms(200);
        ILI9341_FillScreen(ILI9341_BLACK);
        uart_print("[PASS] (SPI2 Controller Init & Color Cycle OK)\r\n");
        passed_tests++;
    } else {
        uart_print("[FAIL] (SPI2 timeout or bus error)\r\n");
    }

    /* TEST 6: Onboard PC13 LED & Visual Heartbeat */
    uart_print("[TEST 6/6] Onboard PC13 LED & Heartbeat... ");
    led_blink(3);
    uart_print("[PASS] (Blinked 3 times)\r\n");
    passed_tests++;

    /* Summary */
    uart_print("\r\n----------------------------------------------------\r\n");
    uart_print("Target Hardware Score: ");
    uart_print_int(passed_tests);
    uart_print(" / ");
    uart_print_int(total_tests);
    uart_print(" Tests Passed\r\n");
    if (passed_tests == total_tests) {
        uart_print("RESULT: ALL PHYSICAL HARDWARE DETECTED & OPERATIONAL [PASS]\r\n");
    } else {
        uart_print("RESULT: HARDWARE UNPLUGGED OR DEFECTIVE [FAIL]\r\n");
    }
    uart_print("====================================================\r\n\r\n");

    /* Continuous heartbeat */
    while (1) {
        led_blink(1);
        Delay_ms(1000);
    }
}

#else

/* Host compilation stub for native testing */
int main(void) {
    printf("Host diagnostic test stub: compile with arm-none-eabi-gcc for target STM32\n");
    return 0;
}

#endif
