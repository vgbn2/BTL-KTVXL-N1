#include "mpu6050.h"
#include "system_time.h"
#include <string.h>

#if defined(USE_STDPERIPH_DRIVER) || defined(STM32F10X)

#define SCL GPIO_Pin_6
#define SDA GPIO_Pin_7
//chỉnh pin sda lên cao
static void sda(int high)
{
    if (high) GPIO_SetBits(GPIOB, SDA);
    else GPIO_ResetBits(GPIOB, SDA);
}
// chinh pin scl xuong thap
static void scl_low(void)
{
    GPIO_ResetBits(GPIOB, SCL);
    Delay_us(5);
}
//chinh pin scl len cao
static int scl_high(void)
{
    uint32_t start = SystemTime_GetMillis();
    GPIO_SetBits(GPIOB, SCL);
    while (!GPIO_ReadInputDataBit(GPIOB, SCL))
    {
        if ((uint32_t)(SystemTime_GetMillis() - start) >= 2U) return IMU_ERR_I2C;
    }
    Delay_us(5);
    return IMU_OK;
}

static int stop(void)
{
    int status;
    scl_low();
    sda(0);
    Delay_us(5);
    status = scl_high();
    sda(1);
    Delay_us(5);
    return status;
}

static int start(void)
{
    sda(1);
    Delay_us(5);
    if (scl_high() != IMU_OK) return IMU_ERR_I2C;
    if (!GPIO_ReadInputDataBit(GPIOB, SDA)) return IMU_ERR_I2C;
    sda(0);
    Delay_us(5);
    scl_low();
    return IMU_OK;
}
//truyen du lieu
static int send_byte(uint8_t value)
{
    int bit;
    int ack;
    for (bit = 7; bit >= 0; bit--)//truyen thi di tu bit 7 den bit 0 , 
    {
        sda((value >> bit) & 1U);
        Delay_us(5);
        if (scl_high() != IMU_OK) return IMU_ERR_I2C;
        scl_low();
    }
    sda(1);
    Delay_us(5);
    if (scl_high() != IMU_OK) return IMU_ERR_I2C;
    ack = GPIO_ReadInputDataBit(GPIOB, SDA);
    scl_low();
    return ack ? IMU_ERR_I2C : IMU_OK;
}
//nhan du lieu
static int receive_byte(uint8_t *value, int ack)
{
    int bit;
    *value = 0;
    sda(1);
    for (bit = 0; bit < 8; bit++)//nhan thi di tu bit 0 den bit 7
    {
        if (scl_high() != IMU_OK) return IMU_ERR_I2C;
        *value = (uint8_t)((*value << 1) | GPIO_ReadInputDataBit(GPIOB, SDA));
        scl_low();
    }
    sda(!ack);
    Delay_us(5);
    if (scl_high() != IMU_OK) return IMU_ERR_I2C;
    scl_low();
    sda(1);
    return IMU_OK;
}
//theo nguyen ly fifo, first in, first out, truyen du lieu vao truoc thi nhan du lieu ra truoc
//ghi du lieu vao thanh ghi cua mpu6050
static int reg_write(uint8_t reg, uint8_t value)
{
    int status = start();
    if (status == IMU_OK) status = send_byte(MPU6050_I2C_ADDR << 1);
    if (status == IMU_OK) status = send_byte(reg);
    if (status == IMU_OK) status = send_byte(value);
    if (stop() != IMU_OK) status = IMU_ERR_I2C;
    return status;
}
//doc du lieu tu thanh ghi cua mpu6050
static int reg_read(uint8_t reg, uint8_t *data, uint16_t length)
{
    uint16_t i;
    int status = start();
    if (status == IMU_OK) status = send_byte(MPU6050_I2C_ADDR << 1);
    if (status == IMU_OK) status = send_byte(reg);
    if (status == IMU_OK) status = start();
    if (status == IMU_OK) status = send_byte((MPU6050_I2C_ADDR << 1) | 1U);
    for (i = 0; i < length && status == IMU_OK; i++)
    {
        status = receive_byte(&data[i], i + 1U < length);
    }
    if (stop() != IMU_OK) status = IMU_ERR_I2C;
    return status;
}
//chuyen doi 2 byte sang int16_t
static int16_t bytes_to_i16(uint8_t msb, uint8_t lsb)
{
    int32_t value = (int32_t)((msb << 8) | lsb);
    if (value >= 32768) value -= 65536;
    return (int16_t)value;
}
//kiem tra ket noi voi mpu6050
bool MPU6050_CheckConnection(void)
{
    uint8_t id;
    return (reg_read(0x75, &id, 1) == IMU_OK && id == 0x68);
    //register who am i cua mpu6050
    //gia tri mac dinh la 0x68
    //4.34, register 117-who am i

}

int MPU6050_Init(void)
{
    GPIO_InitTypeDef gpio;
    int status;
    uint8_t id;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);
    GPIO_SetBits(GPIOB, SCL | SDA);
    gpio.GPIO_Pin   = SCL | SDA;
    gpio.GPIO_Mode  = GPIO_Mode_Out_OD;
    gpio.GPIO_Speed = GPIO_Speed_2MHz;
    GPIO_Init(GPIOB, &gpio);
    Delay_ms(100);

    // register who am i cua mpu6050 (gia tri mac dinh la 0x68)
    // 4.34, register 117 - who am i
    status = reg_read(0x75, &id, 1);
    if (status != IMU_OK) return status;
    if (id != 0x68) return IMU_ERR_DEVICE_ID;

    
    if (reg_write(0x6B, 0x80) != IMU_OK) return IMU_ERR_I2C;
    //4.30 reigister 107, powermanagement 1, bit 7 = 1, reset device
    Delay_ms(100);

    if (reg_write(0x6B, 0x01) != IMU_OK) return IMU_ERR_I2C;
    Delay_ms(100);

    if (reg_write(0x1A, 0x03) != IMU_OK) return IMU_ERR_I2C;

    if (reg_write(0x19, 0x09) != IMU_OK) return IMU_ERR_I2C;


    if (reg_write(0x1B, 0x00) != IMU_OK) return IMU_ERR_I2C;

    if (reg_write(0x1C, 0x00) != IMU_OK) return IMU_ERR_I2C;

    return IMU_OK;
}

int MPU6050_Read(IMU_Data_t *data)
{
    uint8_t raw[14];
    int status;

    if (data == NULL) return IMU_ERR_INVALID_PARAM;
    status = reg_read(0x3B, raw, 14);
    //register cua accel xout high,4.18  register 59 to 64
    if (status != IMU_OK) return status;

    data->ax_g   = bytes_to_i16(raw[0], raw[1]) / 16384.0f;
    data->ay_g   = bytes_to_i16(raw[2], raw[3]) / 16384.0f;
    data->az_g   = bytes_to_i16(raw[4], raw[5]) / 16384.0f;
    data->gx_dps = bytes_to_i16(raw[8], raw[9]) / 131.0f;
    data->gy_dps = bytes_to_i16(raw[10], raw[11]) / 131.0f;
    data->gz_dps = bytes_to_i16(raw[12], raw[13]) / 131.0f;

    return IMU_OK;
}

/* Compatibility functions for modular tests */
bool mpu6050_init(mpu6050_dev_t *dev, uint8_t i2c_addr)
{
    if (!dev) return false;
    dev->i2c_addr = i2c_addr ? i2c_addr : MPU6050_I2C_ADDR_DEFAULT;
    dev->is_initialized = (MPU6050_Init() == IMU_OK);
    return dev->is_initialized;
}

bool mpu6050_read_all(mpu6050_dev_t *dev,
                      int16_t *ax, int16_t *ay, int16_t *az,
                      int16_t *temp,
                      int16_t *gx, int16_t *gy, int16_t *gz)
{
    if (!dev || !dev->is_initialized) return false;
    uint8_t buffer[14];
    if (reg_read(0x3B, buffer, 14) != IMU_OK) return false;
    if (ax) *ax = bytes_to_i16(buffer[0], buffer[1]);
    if (ay) *ay = bytes_to_i16(buffer[2], buffer[3]);
    if (az) *az = bytes_to_i16(buffer[4], buffer[5]);
    if (temp) *temp = bytes_to_i16(buffer[6], buffer[7]);
    if (gx) *gx = bytes_to_i16(buffer[8], buffer[9]);
    if (gy) *gy = bytes_to_i16(buffer[10], buffer[11]);
    if (gz) *gz = bytes_to_i16(buffer[12], buffer[13]);
    return true;
}

#else

__attribute__((weak)) void Delay_ms(uint32_t time_ms) { (void)time_ms; }

// phan nay la cua mo phong, phan tren la chay that
int MPU6050_Init(void) { return IMU_OK; }

int MPU6050_Read(IMU_Data_t *data)
{
    if (!data) return IMU_ERR_INVALID_PARAM;
    data->ax_g   = 0.0f;
    data->ay_g   = 0.0f;
    data->az_g   = 1.0f;
    data->gx_dps = 0.0f;
    data->gy_dps = 0.0f;
    data->gz_dps = 0.0f;
    return IMU_OK;
}
//ktra ket cnoi voi mpu6050, neu ket noi thanh cong tra ve true, neu ket noi that bai tra ve false
bool MPU6050_CheckConnection(void) { return true; }
//khoitao
bool mpu6050_init(mpu6050_dev_t *dev, uint8_t i2c_addr)
{
    if (!dev) return false;
    dev->i2c_addr = i2c_addr ? i2c_addr : MPU6050_I2C_ADDR_DEFAULT;
    dev->is_initialized = true;
    dev->sim_ax = 0;
    dev->sim_ay = 0;
    dev->sim_az = 16384;
    dev->sim_gx = 0;
    dev->sim_gy = 0;
    dev->sim_gz = 0;
    dev->sim_buffer = NULL;
    dev->sim_buffer_len = 0;
    dev->sim_buffer_idx = 0;
    return true;
}
//đọc dữ liệu từ mpu6050, đọc gia tốc, nhiệt độ và gia tốc góc
bool mpu6050_read_all(mpu6050_dev_t *dev,
                      int16_t *ax, int16_t *ay, int16_t *az,
                      int16_t *temp,
                      int16_t *gx, int16_t *gy, int16_t *gz)
{
    if (!dev || !dev->is_initialized) return false;
    int16_t cur_ax = dev->sim_ax;
    int16_t cur_ay = dev->sim_ay;
    int16_t cur_az = dev->sim_az;
    int16_t cur_gx = dev->sim_gx;
    int16_t cur_gy = dev->sim_gy;
    int16_t cur_gz = dev->sim_gz;

    if (dev->sim_buffer && dev->sim_buffer_len > 0) {
        uint16_t idx = dev->sim_buffer_idx % dev->sim_buffer_len;
        cur_ax = dev->sim_buffer[idx][0];
        cur_ay = dev->sim_buffer[idx][1];
        cur_az = dev->sim_buffer[idx][2];
        cur_gx = dev->sim_buffer[idx][3];
        cur_gy = dev->sim_buffer[idx][4];
        cur_gz = dev->sim_buffer[idx][5];
        dev->sim_buffer_idx++;
    }

    if (ax) *ax = cur_ax;
    if (ay) *ay = cur_ay;
    if (az) *az = cur_az;
    if (temp) *temp = 0;
    if (gx) *gx = cur_gx;
    if (gy) *gy = cur_gy;
    if (gz) *gz = cur_gz;
    return true;
}

#endif

/* Shared multi-sample calibration algorithm (identical across hardware & simulation) */
bool mpu6050_calibrate_offsets(mpu6050_dev_t *dev,
                               int16_t *ax_off, int16_t *ay_off, int16_t *az_off,
                               int16_t *gx_off, int16_t *gy_off, int16_t *gz_off,
                               uint16_t num_samples)
{
    if (!dev || !dev->is_initialized || num_samples == 0) return false;
    int32_t sum_ax = 0, sum_ay = 0, sum_az = 0;
    int32_t sum_gx = 0, sum_gy = 0, sum_gz = 0;
    for (uint16_t i = 0; i < num_samples; i++)
    {
        int16_t ax, ay, az, temp, gx, gy, gz;
        if (!mpu6050_read_all(dev, &ax, &ay, &az, &temp, &gx, &gy, &gz)) return false;
        sum_ax += ax;
        sum_ay += ay;
        sum_az += az;
        sum_gx += gx;
        sum_gy += gy;
        sum_gz += gz;
        Delay_ms(5);
    }
    if (ax_off) *ax_off = (int16_t)(sum_ax / num_samples);
    if (ay_off) *ay_off = (int16_t)(sum_ay / num_samples);
    if (az_off) *az_off = (int16_t)((sum_az / num_samples) - 16384);
    if (gx_off) *gx_off = (int16_t)(sum_gx / num_samples);
    if (gy_off) *gy_off = (int16_t)(sum_gy / num_samples);
    if (gz_off) *gz_off = (int16_t)(sum_gz / num_samples);
    return true;
}
