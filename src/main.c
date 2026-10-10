#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "filter.h"
#include "protocol.h"
#include "storage_format.h"
#include "mpu6050.h"
#include "w25qxx.h"

#if defined(STM32F103xB) || defined(USE_HAL_DRIVER)
#include "stm32f1xx_hal.h"

/* Peripheral Handles */
I2C_HandleTypeDef hi2c1;
SPI_HandleTypeDef hspi1;
UART_HandleTypeDef huart1;
TIM_HandleTypeDef htim2;

/* Volatile ISR Flags */
volatile bool flag_100hz_tick = false;
volatile uint8_t uart_rx_byte = 0;
#define RX_BUF_LEN 128
char rx_line_buf[RX_BUF_LEN];
volatile uint8_t rx_line_idx = 0;
volatile bool flag_rx_cmd_ready = false;

void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_I2C1_Init(void);
static void MX_SPI1_Init(void);
static void MX_USART1_UART_Init(void);
static void MX_TIM2_Init(void);

void TIM2_IRQHandler(void) {
    if (__HAL_TIM_GET_FLAG(&htim2, TIM_FLAG_UPDATE) != RESET) {
        if (__HAL_TIM_GET_IT_SOURCE(&htim2, TIM_IT_UPDATE) != RESET) {
            __HAL_TIM_CLEAR_IT(&htim2, TIM_IT_UPDATE);
            flag_100hz_tick = true;
        }
    }
}

void USART1_IRQHandler(void) {
    if (__HAL_UART_GET_FLAG(&huart1, UART_FLAG_RXNE) != RESET) {
        uint8_t ch = (uint8_t)(huart1.Instance->DR & 0xFF);
        if (ch == '\n' || ch == '\r') {
            if (rx_line_idx > 0) {
                rx_line_buf[rx_line_idx] = '\0';
                flag_rx_cmd_ready = true;
                rx_line_idx = 0;
            }
        } else if (rx_line_idx < RX_BUF_LEN - 1) {
            rx_line_buf[rx_line_idx++] = (char)ch;
        }
    }
}

static void uart_send_str(const char *str) {
    HAL_UART_Transmit(&huart1, (uint8_t *)str, (uint16_t)strlen(str), 100);
}

static void uart_send_bytes(const uint8_t *data, uint16_t len) {
    HAL_UART_Transmit(&huart1, (uint8_t *)data, len, 100);
}

#else

/* Host simulation stubs */
static bool flag_100hz_tick = false;
static bool flag_rx_cmd_ready = false;
static char rx_line_buf[128];

static void uart_send_str(const char *str) {
    printf("[UART_TX] %s", str);
}

static void uart_send_bytes(const uint8_t *data, uint16_t len) {
    printf("[UART_BIN_TX] %u bytes, seq=%u, pitch=%d, roll=%d\n",
           len, data[3], (int16_t)((data[6]<<8)|data[5]), (int16_t)((data[8]<<8)|data[7]));
}
#endif

int main(void) {
#if defined(STM32F103xB) || defined(USE_HAL_DRIVER)
    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();
    MX_I2C1_Init();
    MX_SPI1_Init();
    MX_USART1_UART_Init();
    MX_TIM2_Init();
    HAL_TIM_Base_Start_IT(&htim2);
    __HAL_UART_ENABLE_IT(&huart1, UART_IT_RXNE);
#endif

    uart_send_str("\r\n=== BTL-KTVXL-N1 STM32 TILT SENSOR SYSTEM BOOT ===\r\n");

    /* 1. Initialize Complementary Filter */
    imu_filter_t filter;
    imu_filter_init(&filter, 0.98f, 0.01f);

    /* 2. Initialize SPI Flash and load saved configuration */
    bool flash_ok = w25qxx_init();
    if (flash_ok) {
        flash_config_block_t cfg;
        if (w25qxx_load_config(&cfg)) {
            uart_send_str("[FLASH] Valid config loaded from Sector 0.\r\n");
            imu_filter_set_alpha(&filter, cfg.alpha);
            imu_filter_set_offsets(&filter,
                                   cfg.accel_offset_x, cfg.accel_offset_y, cfg.accel_offset_z,
                                   cfg.gyro_offset_x, cfg.gyro_offset_y, cfg.gyro_offset_z);
        } else {
            uart_send_str("[FLASH] No valid config found; using defaults.\r\n");
        }
    } else {
        uart_send_str("[FLASH] W25Q64 SPI communication error!\r\n");
    }

    /* 3. Initialize MPU6050 */
    mpu6050_dev_t imu;
    bool imu_ok = mpu6050_init(&imu, MPU6050_I2C_ADDR_DEFAULT);
    if (imu_ok) {
        uart_send_str("[IMU] MPU6050 initialized successfully at 100 Hz.\r\n");
    } else {
        uart_send_str("[IMU] MPU6050 I2C detection failed!\r\n");
    }

    uint8_t frame_seq = 0;
    uint32_t loop_counter = 0;
    uint8_t telemetry_buf[TELEMETRY_FRAME_LEN];

#if !defined(STM32F103xB) && !defined(USE_HAL_DRIVER)
    /* Host simulation: run 10 iterations to verify pipeline */
    for (int sim_step = 0; sim_step < 10; sim_step++) {
        flag_100hz_tick = true;
#else
    while (1) {
#endif
        /* Check incoming UART commands */
        if (flag_rx_cmd_ready) {
            flag_rx_cmd_ready = false;
            ascii_cmd_t cmd = protocol_parse_ascii_cmd(rx_line_buf);

            switch (cmd.type) {
                case CMD_CALIBRATE_IMU: {
                    uart_send_str("CALIBRATING_IMU_PLEASE_WAIT...\r\n");
                    int16_t ax_o, ay_o, az_o, gx_o, gy_o, gz_o;
                    if (mpu6050_calibrate_offsets(&imu, &ax_o, &ay_o, &az_o, &gx_o, &gy_o, &gz_o, 200)) {
                        imu_filter_set_offsets(&filter, ax_o, ay_o, az_o, gx_o, gy_o, gz_o);
                        imu_filter_reset(&filter);
                        char msg[96];
                        snprintf(msg, sizeof(msg), "CALIBRATION_OK OFFSETS: AX:%d AY:%d AZ:%d GX:%d GY:%d GZ:%d\r\n",
                                 ax_o, ay_o, az_o, gx_o, gy_o, gz_o);
                        uart_send_str(msg);
                    } else {
                        uart_send_str("CALIBRATION_FAILED\r\n");
                    }
                    break;
                }

                case CMD_SET_FILTER_ALPHA: {
                    if (imu_filter_set_alpha(&filter, cmd.param_float)) {
                        char msg[64];
                        snprintf(msg, sizeof(msg), "SET_FILTER_ALPHA_OK:%.4f\r\n", cmd.param_float);
                        uart_send_str(msg);
                    } else {
                        uart_send_str("SET_FILTER_ALPHA_INVALID_RANGE\r\n");
                    }
                    break;
                }

                case CMD_GET_ANGLE: {
                    char msg[64];
                    snprintf(msg, sizeof(msg), "ROLL:%.2f PITCH:%.2f YAW:%.2f\r\n",
                             filter.roll, filter.pitch, filter.yaw);
                    uart_send_str(msg);
                    break;
                }

                case CMD_SAVE_FLASH: {
                    flash_config_block_t cfg;
                    memset(&cfg, 0, sizeof(cfg));
                    cfg.alpha = filter.alpha;
                    cfg.accel_offset_x = filter.accel_offset_x;
                    cfg.accel_offset_y = filter.accel_offset_y;
                    cfg.accel_offset_z = filter.accel_offset_z;
                    cfg.gyro_offset_x = filter.gyro_offset_x;
                    cfg.gyro_offset_y = filter.gyro_offset_y;
                    cfg.gyro_offset_z = filter.gyro_offset_z;
                    if (w25qxx_save_config(&cfg)) {
                        uart_send_str("FLASH_SAVE_SUCCESS\r\n");
                    } else {
                        uart_send_str("FLASH_SAVE_FAILED\r\n");
                    }
                    break;
                }

                case CMD_LOAD_FLASH: {
                    flash_config_block_t cfg;
                    if (w25qxx_load_config(&cfg)) {
                        imu_filter_set_alpha(&filter, cfg.alpha);
                        imu_filter_set_offsets(&filter,
                                               cfg.accel_offset_x, cfg.accel_offset_y, cfg.accel_offset_z,
                                               cfg.gyro_offset_x, cfg.gyro_offset_y, cfg.gyro_offset_z);
                        uart_send_str("FLASH_LOAD_SUCCESS\r\n");
                    } else {
                        uart_send_str("FLASH_LOAD_CRC_ERROR\r\n");
                    }
                    break;
                }

                default:
                    uart_send_str("ERR_UNKNOWN_COMMAND\r\n");
                    break;
            }
        }

        /* 100 Hz Periodic Sensor Sampling & Filtering Loop */
        if (flag_100hz_tick) {
            flag_100hz_tick = false;

            int16_t ax, ay, az, temp, gx, gy, gz;
            if (mpu6050_read_all(&imu, &ax, &ay, &az, &temp, &gx, &gy, &gz)) {
                imu_filter_update(&filter, ax, ay, az, gx, gy, gz);

                /* Transmit binary telemetry frame to ESP32 */
                uint16_t flags = (filter.is_calibrated ? 0x01 : 0x00) | (flash_ok ? 0x02 : 0x00);
                if (protocol_encode_telemetry(telemetry_buf, sizeof(telemetry_buf),
                                              frame_seq++,
                                              filter.pitch, filter.roll, filter.yaw,
                                              flags)) {
                    uart_send_bytes(telemetry_buf, TELEMETRY_FRAME_LEN);
                }
            }

            loop_counter++;
#if defined(STM32F103xB) || defined(USE_HAL_DRIVER)
            /* Toggle onboard PC13 LED heartbeat every 500 ms (50 ticks) */
            if (loop_counter % 50 == 0) {
                HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_13);
            }
#endif
        }
    }

    return 0;
}
