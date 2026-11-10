#include "uart_cmd.h"
#include "calibration.h"
#include "angle_filter.h"
#include "flash_config.h"
#include "display_system.h"
#include "system_time.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>

#if defined(USE_STDPERIPH_DRIVER) || defined(STM32F10X)

/* ===== Cau hinh USART1 ===== */
#define UART_PORT USART1
#define UART_BAUDRATE 115200
#define RX_BUFFER_SIZE 128U
#define QUEUE_SIZE 4U

/* Bien noi bo module */
static volatile char commands[QUEUE_SIZE][RX_BUFFER_SIZE];
static volatile uint8_t queue_write = 0, queue_read = 0;
static volatile uint8_t receive_error = 0;
static uint16_t rx_index = 0;
static uint8_t discard_line = 0;

/* ===== Ham noi bo: gui chuoi qua UART (polling voi timeout) ===== */
static void uart_send_string(const char *text)
{
    while (*text)
    {
        uint32_t start = SystemTime_GetMillis();
        while (USART_GetFlagStatus(UART_PORT, USART_FLAG_TXE) == RESET)
        {
            if ((uint32_t)(SystemTime_GetMillis() - start) >= 20U)
            {
                return;
            }
        }
        USART_SendData(UART_PORT, (uint8_t)*text++);
    }
}

static void uart_send_float(const char *prefix, float value, unsigned decimals, const char *suffix)
{
    char output[96];
    uint32_t scale = (decimals == 3U) ? 1000U : 10U;
    uint32_t magnitude = (uint32_t)(fabsf(value) * scale + 0.5f);
    snprintf(output, sizeof(output), "%s%s%lu.%0*lu%s", prefix,
             value < 0.0f ? "-" : "", (unsigned long)(magnitude / scale),
             (int)decimals, (unsigned long)(magnitude % scale), suffix);
    uart_send_string(output);
}

/* ===== API cong khai ===== */
void UART_Init(void)
{
    GPIO_InitTypeDef gpio;
    USART_InitTypeDef usart;
    NVIC_InitTypeDef nvic;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_USART1, ENABLE);

    /* PA9 = TX: AF_PP */
    gpio.GPIO_Pin   = GPIO_Pin_9;
    gpio.GPIO_Mode  = GPIO_Mode_AF_PP;
    gpio.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &gpio);

    /* PA10 = RX: IN_FLOATING */
    gpio.GPIO_Pin  = GPIO_Pin_10;
    gpio.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(GPIOA, &gpio);

    usart.USART_BaudRate            = UART_BAUDRATE;
    usart.USART_WordLength          = USART_WordLength_8b;
    usart.USART_StopBits            = USART_StopBits_1;
    usart.USART_Parity              = USART_Parity_No;
    usart.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    usart.USART_Mode                = USART_Mode_Rx | USART_Mode_Tx;
    USART_Init(UART_PORT, &usart);

    USART_ITConfig(UART_PORT, USART_IT_RXNE, ENABLE);

    nvic.NVIC_IRQChannel                   = USART1_IRQn;
    nvic.NVIC_IRQChannelPreemptionPriority = 1;
    nvic.NVIC_IRQChannelSubPriority        = 1;
    nvic.NVIC_IRQChannelCmd                = ENABLE;
    NVIC_Init(&nvic);

    USART_Cmd(UART_PORT, ENABLE);
}

/* ===== Trinh xu ly lenh ===== */
static void process_command(const char *command)
{
    /* 1. INFO_REQUEST */
    if (strcmp(command, "INFO_REQUEST") == 0 || strcmp(command, "INFO_REQUEST:GROUP") == 0)
    {
        UART_SendGroupInfo();
    }
    /* 2. CALIBRATE_IMU */
    else if (strcmp(command, "CALIBRATE_IMU") == 0)
    {
        System_Config_t config;
        int status;
        DisplaySystem_SetState(SYSTEM_CALIBRATING);
        status = Calibration_Run();
        if (status != IMU_OK)
        {
            uart_send_string("ERROR:CALIBRATE_FAILED\r\n");
            DisplaySystem_SetState(SYSTEM_ERROR);
        }
        else
        {
            Calibration_GetOffset(&config.offset);
            config.alpha = Angle_GetAlpha();
            status = Flash_SaveConfig(&config);
            if (status == IMU_OK)
            {
                uart_send_string("CALIBRATE:OK\r\n");
            }
            else
            {
                uart_send_string("ERROR:FLASH_SAVE_FAILED;CALIBRATION_IN_RAM\r\n");
            }
            uart_send_float("ACC_OFFSET:", config.offset.accel_offset_x, 3, ",");
            uart_send_float("", config.offset.accel_offset_y, 3, ",");
            uart_send_float("", config.offset.accel_offset_z, 3, "\r\n");
            uart_send_float("GYRO_OFFSET:", config.offset.gyro_offset_x, 3, ",");
            uart_send_float("", config.offset.gyro_offset_y, 3, ",");
            uart_send_float("", config.offset.gyro_offset_z, 3, "\r\n");
            DisplaySystem_SetState(SYSTEM_READY);
        }
        DisplaySystem_Resync();
    }
    /* 3. SET_FILTER_ALPHA:<val> */
    else if (strncmp(command, "SET_FILTER_ALPHA:", 17) == 0)
    {
        char *end;
        float alpha = (float)strtod(command + 17, &end);
        System_Config_t config;
        if (end == command + 17 || *end != '\0' || Angle_SetAlpha(alpha) != IMU_OK)
        {
            uart_send_string("ERROR:INVALID_ALPHA\r\n");
            return;
        }

        if (!Calibration_IsCalibrated())
        {
            uart_send_string("STATUS:RAM_ONLY;CALIBRATE_TO_SAVE\r\n");
        }
        else
        {
            Calibration_GetOffset(&config.offset);
            config.alpha = alpha;
            if (Flash_SaveConfig(&config) != IMU_OK)
            {
                uart_send_string("ERROR:FLASH_SAVE_FAILED\r\n");
            }
        }
        uart_send_float("FILTER_ALPHA:", alpha, 3, "\r\n");
        DisplaySystem_Resync();
    }
    /* 4. GET_ANGLE */
    else if (strcmp(command, "GET_ANGLE") == 0)
    {
        Angle_Data_t angle;
        if (!DisplaySystem_HasAngle())
        {
            uart_send_string("ERROR:ANGLE_NOT_READY\r\n");
        }
        else
        {
            Angle_Get(&angle);
            UART_SendAngle(&angle);
        }
    }
    else
    {
        uart_send_string("ERROR:UNKNOWN_CMD\r\n");
    }
}

/* ===== API cong khai: xu ly lenh trong main loop ===== */
void UART_Process(void)
{
    char command[RX_BUFFER_SIZE];
    uint16_t i;
    uint8_t error;
    uint32_t mask = __get_PRIMASK();
    __disable_irq();
    error = receive_error;
    receive_error = 0;
    __set_PRIMASK(mask);

    if (error)
    {
        uart_send_string("ERROR:UART_OVERFLOW_OR_FRAME\r\n");
    }

    if (queue_read == queue_write)
    {
        return;
    }

    for (i = 0; i < RX_BUFFER_SIZE; i++)
    {
        command[i] = commands[queue_read][i];
        if (command[i] == '\0') break;
    }
    command[RX_BUFFER_SIZE - 1] = '\0';
    queue_read = (uint8_t)((queue_read + 1U) % QUEUE_SIZE);
    process_command(command);
}

void UART_SendAngle(const Angle_Data_t *angle)
{
    if (angle == NULL) return;
    uart_send_float("ROLL:", angle->roll_deg, 1, "\r\n");
    uart_send_float("PITCH:", angle->pitch_deg, 1, "\r\n");
}

void UART_SendGroupInfo(void)
{
    uart_send_string("GROUP_NAME: Nhom 1\r\n");
    uart_send_string("MEMBERS: Tran Duc Anh, Tran Quoc An, Nguyen Duy Anh, Tran Tuan Anh, Tran Khac Anh\r\n");
    uart_send_string("PROJECT_NAME: MPU6050 Inclinometer\r\n");
    uart_send_string("HARDWARE_USED: STM32F103C8, MPU6050, ILI9341, W25Qxx\r\n");
    uart_send_string("SOFTWARE_USED: Keil MDK, SPL\r\n");
    uart_send_string("DATE: " __DATE__ "\r\n");
}

/* ===== Trinh ngat USART1 ===== */
void USART1_IRQHandler(void)
{
    uint32_t status = UART_PORT->SR;
    char ch;
    uint8_t next;

    if (!(status & (USART_SR_RXNE | USART_SR_ORE | USART_SR_FE | USART_SR_NE)))
    {
        return;
    }

    ch = (char)UART_PORT->DR;
    if (status & (USART_SR_ORE | USART_SR_FE | USART_SR_NE))
    {
        discard_line = 1;
        receive_error = 1;
    }

    if (!(status & USART_SR_RXNE))
    {
        return;
    }

    if (ch == '\r' || ch == '\n')
    {
        if (!discard_line && rx_index != 0U)
        {
            commands[queue_write][rx_index] = '\0';
            __DMB();
            queue_write = (uint8_t)((queue_write + 1U) % QUEUE_SIZE);
        }
        rx_index = 0;
        discard_line = 0;
        return;
    }

    next = (uint8_t)((queue_write + 1U) % QUEUE_SIZE);
    if (discard_line) return;
    if (next == queue_read || rx_index >= RX_BUFFER_SIZE - 1U)
    {
        discard_line = 1;
        receive_error = 1;
        return;
    }

    commands[queue_write][rx_index++] = ch;
}

#else

/* Host simulation stubs */
void UART_Init(void) { }
void UART_Process(void) { }
void UART_SendAngle(const Angle_Data_t *angle)
{
    if (angle) {
        printf("ROLL:%.1f\r\nPITCH:%.1f\r\n", angle->roll_deg, angle->pitch_deg);
    }
}
void UART_SendGroupInfo(void)
{
    printf("GROUP_NAME: Nhom 1\r\n");
    printf("MEMBERS: Tran Duc Anh, Tran Quoc An, Nguyen Duy Anh, Tran Tuan Anh, Tran Khac Anh\r\n");
    printf("PROJECT_NAME: MPU6050 Inclinometer\r\n");
    printf("HARDWARE_USED: STM32F103C8, MPU6050, ILI9341, W25Qxx\r\n");
    printf("SOFTWARE_USED: Keil MDK, SPL\r\n");
}

#endif
