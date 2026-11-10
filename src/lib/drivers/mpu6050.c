#include "mpu6050.h"
#include "system_time.h"
 
 
#define SCL GPIO_Pin_6
#define SDA GPIO_Pin_7
static void sda(int high)
{
    if (high) GPIO_SetBits(GPIOB, SDA);
    else GPIO_ResetBits(GPIOB, SDA);
}
static void scl_low(void) { GPIO_ResetBits(GPIOB, SCL); Delay_us(5); }
static int scl_high(void)
{
    uint32_t start = SystemTime_GetMillis();
    GPIO_SetBits(GPIOB, SCL);
    while (!GPIO_ReadInputDataBit(GPIOB, SCL))
        if ((uint32_t)(SystemTime_GetMillis() - start) >= 2U) return IMU_ERR_I2C;
    Delay_us(5);
    return IMU_OK;
}
static int stop(void)
{
    int status;
    scl_low(); sda(0); Delay_us(5);
    status = scl_high(); sda(1); Delay_us(5);
    return status;
}
static int start(void)
{
    sda(1); Delay_us(5);
    if (scl_high() != IMU_OK) return IMU_ERR_I2C;
    if (!GPIO_ReadInputDataBit(GPIOB, SDA)) return IMU_ERR_I2C;
    sda(0); Delay_us(5); scl_low();
    return IMU_OK;
}
static int send_byte(uint8_t value)
{
    int bit;
    int ack;
    for (bit = 7; bit >= 0; bit--)
    {
        sda((value >> bit) & 1U); Delay_us(5);
        if (scl_high() != IMU_OK) return IMU_ERR_I2C;
        scl_low();
    }
    sda(1); Delay_us(5);
    if (scl_high() != IMU_OK) return IMU_ERR_I2C;
    ack = GPIO_ReadInputDataBit(GPIOB, SDA);
    scl_low();
    return ack ? IMU_ERR_I2C : IMU_OK;
}
static int receive_byte(uint8_t *value, int ack)
{
    int bit;
    *value = 0;
    sda(1);
    for (bit = 0; bit < 8; bit++)
    {
        if (scl_high() != IMU_OK) return IMU_ERR_I2C;
        *value = (uint8_t)((*value << 1) | GPIO_ReadInputDataBit(GPIOB, SDA));
        scl_low();
    }
    sda(!ack); Delay_us(5);
    if (scl_high() != IMU_OK) return IMU_ERR_I2C;
    scl_low(); sda(1);
    return IMU_OK;
}

// mem write check
static int reg_write(uint8_t reg, uint8_t value)
{
    int status = start();
    if (status == IMU_OK) status = send_byte(MPU6050_I2C_ADDR << 1);
    if (status == IMU_OK) status = send_byte(reg);
    if (status == IMU_OK) status = send_byte(value);
    if (stop() != IMU_OK) status = IMU_ERR_I2C;
    return status;
}

// mem read check
static int reg_read(uint8_t reg, uint8_t *data, uint16_t length)
{
    uint16_t i;
    int status = start();
    if (status == IMU_OK) status = send_byte(MPU6050_I2C_ADDR << 1);
    if (status == IMU_OK) status = send_byte(reg);
    if (status == IMU_OK) status = start();
    if (status == IMU_OK) status = send_byte((MPU6050_I2C_ADDR << 1) | 1U);
    for (i = 0; i < length && status == IMU_OK; i++)
        status = receive_byte(&data[i], i + 1U < length);
    if (stop() != IMU_OK) status = IMU_ERR_I2C;
    return status;
}
bool MPU6050_CheckConnection(void)
{
    uint8_t id;
    return reg_read(0x75, &id, 1) == IMU_OK && id == 0x68;
}
int MPU6050_Init(void)
{
    GPIO_InitTypeDef gpio;
    int status;
    uint8_t id;
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);
    GPIO_SetBits(GPIOB, SCL | SDA);
    gpio.GPIO_Pin = SCL | SDA;
    gpio.GPIO_Mode = GPIO_Mode_Out_OD;
    gpio.GPIO_Speed = GPIO_Speed_2MHz;
    GPIO_Init(GPIOB, &gpio);
    Delay_ms(100);
    
    // MPU6050 WHO_AM_I_VAL
    status = reg_read(0x75, &id, 1);
    if (status != IMU_OK) return status;
    if (id != 0x68) return IMU_ERR_DEVICE_ID;
    
    // reset toan bo thiet bi
    
    // POWER_MANAGEMENT_1 bit 7 la reset (4.30 reference cua mpu 6050)
    
    // day la cong doan tat
    
    // 7 bit->hex=0x80
    status = reg_write(0x6B, 0x80);
    if (status != IMU_OK) return status;
    
    // day la cong doan bat
    Delay_ms(100);
    status = reg_write(0x6B, 0x01);
    if (status != IMU_OK) return status;
    Delay_ms(100);
    
    // bo loc thong thap
    if (reg_write(0x1A, 0x03) != IMU_OK) return IMU_ERR_I2C;
    
    // dat tan so lay mau( sample rate)
    if (reg_write(0x19, 0x09) != IMU_OK) return IMU_ERR_I2C;
    
    // config cam bien con quay
    if (reg_write(0x1B, 0x00) != IMU_OK) return IMU_ERR_I2C;
    
    // config cam bien gia toc
    if (reg_write(0x1C, 0x00) != IMU_OK) return IMU_ERR_I2C;
    return IMU_OK;
}
static int16_t bytes_to_i16(uint8_t high, uint8_t low)
{
    uint16_t value = ((uint16_t)high << 8) | low;
    int32_t signed_value = value;
    if (value >= 32768U) signed_value -= 65536;
    return (int16_t)signed_value;
}
int MPU6050_Read(IMU_Data_t *data)
{
    uint8_t raw[14];
    int status;
    
    // ko co du lieu ->baos thao so loi
    if (data == 0) return IMU_ERR_INVALID_PARAM;
    status = reg_read(0x3B, raw, 14);
    if (status != IMU_OK) return status;
    data->ax_g = bytes_to_i16(raw[0], raw[1]) / 16384.0f;
    data->ay_g = bytes_to_i16(raw[2], raw[3]) / 16384.0f;
    data->az_g = bytes_to_i16(raw[4], raw[5]) / 16384.0f;
    data->gx_dps = bytes_to_i16(raw[8], raw[9]) / 131.0f;
    data->gy_dps = bytes_to_i16(raw[10], raw[11]) / 131.0f;
    data->gz_dps = bytes_to_i16(raw[12], raw[13]) / 131.0f;
    return IMU_OK;
}
