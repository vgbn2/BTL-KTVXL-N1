#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <math.h>

#include "filter.h"
#include "protocol.h"
#include "storage_format.h"
#include "mpu6050.h"
#include "w25qxx.h"

#define UART_PIPE_SIZE 2048

typedef struct {
    uint8_t buffer[UART_PIPE_SIZE];
    uint16_t head;
    uint16_t tail;
} uart_pipe_t;

static void pipe_init(uart_pipe_t *p) {
    p->head = 0;
    p->tail = 0;
}

static void pipe_write(uart_pipe_t *p, const uint8_t *data, uint16_t len) {
    for (uint16_t i = 0; i < len; i++) {
        p->buffer[p->head] = data[i];
        p->head = (p->head + 1) % UART_PIPE_SIZE;
    }
}

static uint16_t pipe_available(const uart_pipe_t *p) {
    if (p->head >= p->tail) return p->head - p->tail;
    return UART_PIPE_SIZE - (p->tail - p->head);
}

static uint8_t pipe_read_byte(uart_pipe_t *p) {
    if (p->head == p->tail) return 0;
    uint8_t b = p->buffer[p->tail];
    p->tail = (p->tail + 1) % UART_PIPE_SIZE;
    return b;
}

int main(void) {
    int failed = 0;
    int total_checks = 0;

    printf("====================================================\n");
    printf("  COMPONENT TEST: END-TO-END PIPELINE INTEGRATION   \n");
    printf("====================================================\n");

    /* 1. Initialize All Subsystem Modules */
    printf("[STEP 1] Initializing Subsystems (IMU, Filter, Flash, UART Pipes)...\n");
    mpu6050_dev_t imu;
    total_checks++;
    if (!mpu6050_init(&imu, MPU6050_I2C_ADDR_DEFAULT)) failed++;

    total_checks++;
    if (!w25qxx_init()) failed++;

    imu_filter_t filter;
    imu_filter_init(&filter, 0.98f, 0.01f);

    uart_pipe_t stm32_to_esp32_pipe;
    uart_pipe_t esp32_to_stm32_pipe;
    pipe_init(&stm32_to_esp32_pipe);
    pipe_init(&esp32_to_stm32_pipe);

    /* 2. Load Stored Configuration from SPI Flash */
    printf("[STEP 2] Loading Configuration from W25Q64 Sector 0...\n");
    flash_config_block_t config;
    if (!w25qxx_load_config(&config)) {
        printf("  -> Flash uninitialized, applying and saving defaults...\n");
        memset(&config, 0, sizeof(config));
        config.magic = FLASH_CONFIG_MAGIC;
        config.version = FLASH_CONFIG_VERSION;
        config.alpha = 0.98f;
        config.write_count = 1;
        storage_seal_config(&config);
        total_checks++;
        if (!w25qxx_save_config(&config)) failed++;
    }
    total_checks++;
    if (config.magic != FLASH_CONFIG_MAGIC) failed++;

    imu_filter_set_alpha(&filter, config.alpha);
    imu_filter_set_offsets(&filter, config.accel_offset_x, config.accel_offset_y, config.accel_offset_z,
                                  config.gyro_offset_x, config.gyro_offset_y, config.gyro_offset_z);

    /* 3. Simulate Sensor Acquisition -> 100 Hz Filter -> Telemetry Encode -> UART TX */
    printf("[STEP 3] Running 100 Hz Acquisition & Telemetry Pipeline (50 ticks)...\n");
    uint8_t telemetry_tx_buf[TELEMETRY_FRAME_LEN];
    uint8_t seq = 1;

    /* Inject a synthetic 25.0 degree pitch tilt into simulated IMU registers (DIN 9300: Y-axis rotation) */
    /* ax = -16384 * sin(25 deg) = -6924, az = 16384 * cos(25 deg) = 14849 */
    imu.sim_ax = -6924;
    imu.sim_ay = 0;
    imu.sim_az = 14849;
    imu.sim_gx = 0;
    imu.sim_gy = 0;
    imu.sim_gz = 0;

    for (int tick = 0; tick < 50; tick++) {
        int16_t ax = 0, ay = 0, az = 0, temp = 0, gx = 0, gy = 0, gz = 0;
        total_checks++;
        if (!mpu6050_read_all(&imu, &ax, &ay, &az, &temp, &gx, &gy, &gz)) failed++;
        total_checks++;
        if (ax != -6924 || az != 14849) failed++;

        imu_filter_update(&filter, ax, ay, az, gx, gy, gz);

        uint16_t status_flags = 0x0001; /* Sensor OK */
        total_checks++;
        if (!protocol_encode_telemetry(telemetry_tx_buf, sizeof(telemetry_tx_buf),
                                       seq++, filter.pitch, filter.roll, filter.yaw, status_flags)) {
            failed++;
        }

        pipe_write(&stm32_to_esp32_pipe, telemetry_tx_buf, TELEMETRY_FRAME_LEN);
    }

    /* 4. Host Receiver Node Ingest & Frame Parsing */
    printf("[STEP 4] Host Node Ingesting & Validating Telemetry Frames...\n");
    uint8_t rx_byte_stream[TELEMETRY_FRAME_LEN];
    uint32_t frames_received = 0;

    while (pipe_available(&stm32_to_esp32_pipe) >= TELEMETRY_FRAME_LEN) {
        for (int i = 0; i < TELEMETRY_FRAME_LEN; i++) {
            rx_byte_stream[i] = pipe_read_byte(&stm32_to_esp32_pipe);
        }
        telemetry_frame_t frame;
        total_checks++;
        if (protocol_decode_telemetry(rx_byte_stream, TELEMETRY_FRAME_LEN, &frame)) {
            frames_received++;
            if (frames_received == 50) {
                float decoded_pitch = frame.pitch_x100 / 100.0f;
                printf("  -> Frame #%u decoded: Pitch=%.2f deg, Roll=%.2f deg, Flags=0x%04X\n",
                       frame.seq, decoded_pitch, frame.roll_x100 / 100.0f, frame.status_flag);
                total_checks++;
                if (decoded_pitch <= 10.0f || decoded_pitch > 26.0f) failed++;
            }
        } else {
            failed++;
        }
    }
    total_checks++;
    if (frames_received != 50) failed++;

    /* 5. Bi-directional Remote Command Execution Pipeline */
    printf("[STEP 5] Testing Remote Command Pipeline: Host -> STM32 -> Flash -> Telemetry...\n");
    const char *cmd_str = "SET_FILTER_ALPHA:0.92\n";
    pipe_write(&esp32_to_stm32_pipe, (const uint8_t *)cmd_str, (uint16_t)strlen(cmd_str));

    /* STM32 parses command from UART */
    char cmd_buf[64];
    uint16_t cmd_idx = 0;
    while (pipe_available(&esp32_to_stm32_pipe) > 0 && cmd_idx < sizeof(cmd_buf) - 1) {
        cmd_buf[cmd_idx++] = (char)pipe_read_byte(&esp32_to_stm32_pipe);
    }
    cmd_buf[cmd_idx] = '\0';

    ascii_cmd_t cmd = protocol_parse_ascii_cmd(cmd_buf);
    total_checks++;
    if (cmd.type != CMD_SET_FILTER_ALPHA) failed++;
    total_checks++;
    if (fabsf(cmd.param_float - 0.92f) >= 1e-4) failed++;

    /* Apply new alpha to filter and save to W25Q64 Flash */
    total_checks++;
    if (!imu_filter_set_alpha(&filter, cmd.param_float)) failed++;
    config.alpha = filter.alpha;
    config.write_count++;
    storage_seal_config(&config);
    total_checks++;
    if (!w25qxx_save_config(&config)) failed++;

    /* Reload from flash to verify durability */
    flash_config_block_t verified_cfg;
    total_checks++;
    if (!w25qxx_load_config(&verified_cfg)) failed++;
    total_checks++;
    if (fabsf(verified_cfg.alpha - 0.92f) >= 1e-4) failed++;
    total_checks++;
    if (verified_cfg.write_count != config.write_count) failed++;

    printf("\n====================================================\n");
    if (failed == 0) {
        printf("   COMPONENT PIPELINE INTEGRATION TEST: PASSED (%d checks) ✅\n", total_checks);
        printf("====================================================\n");
        return 0;
    } else {
        fprintf(stderr, "   COMPONENT PIPELINE INTEGRATION TEST: FAILED (%d/%d failed) ❌\n", failed, total_checks);
        fprintf(stderr, "====================================================\n");
        return 1;
    }
}
