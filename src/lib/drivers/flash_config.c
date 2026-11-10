#include "flash_config.h"
#include "system_time.h"
#include <stddef.h>
#include <string.h>
#include <float.h>

// ===== Cau hinh chan SPI1 cho W25Qxx =====
static int flash_ready = 0;
static int spi_error = 0;
static void select_flash(int selected)
{
    if (selected) GPIO_ResetBits(GPIOA, GPIO_Pin_4);
    else GPIO_SetBits(GPIOA, GPIO_Pin_4);
}

// ===== Ham noi bo: truyen/nhan 1 byte qua SPI =====
static uint8_t spi_transfer(uint8_t data)
{
    uint32_t start = SystemTime_GetMillis();
    if (spi_error) return 0xFF;
    while (SPI_I2S_GetFlagStatus(SPI1, SPI_I2S_FLAG_TXE) == RESET)
        if ((uint32_t)(SystemTime_GetMillis() - start) >= 10U) { spi_error = 1; return 0xFF; }
    SPI_I2S_SendData(SPI1, data);
    while (SPI_I2S_GetFlagStatus(SPI1, SPI_I2S_FLAG_RXNE) == RESET)
        if ((uint32_t)(SystemTime_GetMillis() - start) >= 10U) { spi_error = 1; return 0xFF; }
    return (uint8_t)SPI_I2S_ReceiveData(SPI1);
}
static uint8_t read_status(void)
{
    uint8_t value;
    select_flash(1); spi_transfer(0x05); value = spi_transfer(0xFF); select_flash(0);
    return value;
}

// ===== Ham noi bo: cho chip ranh =====
static int wait_ready(uint32_t timeout_ms)
{
    uint32_t start = SystemTime_GetMillis();
    do
    {
        uint8_t status = read_status();
        if (spi_error) return IMU_ERR_FLASH;
        if (!(status & 1U)) return IMU_OK;
    } while ((uint32_t)(SystemTime_GetMillis() - start) < timeout_ms);
    return IMU_ERR_FLASH;
}
static int write_enable(void)
{
    select_flash(1); spi_transfer(0x06); select_flash(0);
    if (!(read_status() & 2U) || spi_error) return IMU_ERR_FLASH;
    return IMU_OK;
}
static void send_address(uint32_t address)
{
    spi_transfer((uint8_t)(address >> 16));
    spi_transfer((uint8_t)(address >> 8));
    spi_transfer((uint8_t)address);
}
static int read_config(System_Config_t *config)
{
    uint8_t *bytes = (uint8_t *)config;
    uint32_t i;
    if (wait_ready(1000U) != IMU_OK) return IMU_ERR_FLASH;
    select_flash(1); spi_transfer(0x03); send_address(FLASH_CONFIG_BASE_ADDR);
    for (i = 0; i < sizeof(*config); i++) bytes[i] = spi_transfer(0xFF);
    select_flash(0);
    return spi_error ? IMU_ERR_FLASH : IMU_OK;
}
static uint32_t config_crc(const System_Config_t *config)
{
    const uint8_t *bytes = (const uint8_t *)config;
    uint32_t crc = 0xFFFFFFFFUL;
    uint32_t i, bit;
    for (i = 0; i < offsetof(System_Config_t, crc32); i++)
    {
        crc ^= bytes[i];
        for (bit = 0; bit < 8; bit++)
            crc = (crc >> 1) ^ ((crc & 1U) ? 0xEDB88320UL : 0U);
    }
    return ~crc;
}
static int valid_config(const System_Config_t *config)
{
    const IMU_Offset_t *o = &config->offset;
    if (!(config->alpha >= IMU_ALPHA_MIN && config->alpha <= IMU_ALPHA_MAX)) return 0;
     
    return o->accel_offset_x >= -2.0f && o->accel_offset_x <= 2.0f
        && o->accel_offset_y >= -2.0f && o->accel_offset_y <= 2.0f
        && o->accel_offset_z >= -3.0f && o->accel_offset_z <= 1.0f
        && o->gyro_offset_x >= -250.0f && o->gyro_offset_x <= 250.0f
        && o->gyro_offset_y >= -250.0f && o->gyro_offset_y <= 250.0f
        && o->gyro_offset_z >= -250.0f && o->gyro_offset_z <= 250.0f;
}

// ===== API cong khai =====
int Flash_Init(void)
{
    GPIO_InitTypeDef gpio;
    SPI_InitTypeDef spi;
    uint8_t manufacturer, type, capacity;
    flash_ready = 0; spi_error = 0;
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_SPI1, ENABLE);
    GPIO_SetBits(GPIOA, GPIO_Pin_4);
    
    // PA4 = CS: Out_PP
    gpio.GPIO_Pin = GPIO_Pin_4; gpio.GPIO_Mode = GPIO_Mode_Out_PP;
    gpio.GPIO_Speed = GPIO_Speed_50MHz; GPIO_Init(GPIOA, &gpio);
    
    // PA5 = SCK, PA7 = MOSI: AF_PP
    gpio.GPIO_Pin = GPIO_Pin_5 | GPIO_Pin_7; gpio.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_Init(GPIOA, &gpio);
    
    // PA6 = MISO: IN_FLOATING
    gpio.GPIO_Pin = GPIO_Pin_6; gpio.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(GPIOA, &gpio);
    SPI_StructInit(&spi);
    spi.SPI_Mode = SPI_Mode_Master; spi.SPI_NSS = SPI_NSS_Soft;
    spi.SPI_BaudRatePrescaler = SPI_BaudRatePrescaler_8;
    SPI_Init(SPI1, &spi); SPI_Cmd(SPI1, ENABLE);
     
    select_flash(1); spi_transfer(0xAB); select_flash(0); Delay_ms(1);
    if (wait_ready(1000) != IMU_OK) return IMU_ERR_FLASH;
    
    // Doc JEDEC ID: W25Q64 → 0xEF4017
    select_flash(1); spi_transfer(0x9F);
    manufacturer = spi_transfer(0xFF); type = spi_transfer(0xFF); capacity = spi_transfer(0xFF);
    select_flash(0);
     
    if (spi_error
        || !((manufacturer == 0xEF && (type == 0x40 || type == 0x60)
              && capacity >= 0x14 && capacity <= 0x18)
             || (manufacturer == 0x20 && type == 0x40 && capacity == 0x16)))
        return IMU_ERR_FLASH;
    flash_ready = 1;
    return IMU_OK;
}
int Flash_SaveConfig(const System_Config_t *config)
{
    System_Config_t saved, check;
    const uint8_t *bytes;
    uint32_t i;
    if (config == 0 || !valid_config(config)) return IMU_ERR_INVALID_PARAM;
    if (!flash_ready) return IMU_ERR_FLASH;
    spi_error = 0;
    memset(&saved, 0, sizeof(saved));
    saved.magic = IMU_CONFIG_MAGIC; saved.offset = config->offset; saved.alpha = config->alpha;
    saved.crc32 = config_crc(&saved);
    if (wait_ready(1000) != IMU_OK || write_enable() != IMU_OK) return IMU_ERR_FLASH;
    select_flash(1); spi_transfer(0x20); send_address(FLASH_CONFIG_BASE_ADDR); select_flash(0);
    if (wait_ready(3000) != IMU_OK || write_enable() != IMU_OK) return IMU_ERR_FLASH;
     
    bytes = (const uint8_t *)&saved;
    select_flash(1); spi_transfer(0x02); send_address(FLASH_CONFIG_BASE_ADDR);
    for (i = 0; i < sizeof(saved); i++) spi_transfer(bytes[i]);
    select_flash(0);
    if (wait_ready(1000) != IMU_OK || read_config(&check) != IMU_OK) return IMU_ERR_FLASH;
    if (memcmp(&check, &saved, sizeof(saved)) != 0) return IMU_ERR_FLASH;
    return IMU_OK;
}
int Flash_LoadConfig(System_Config_t *config)
{
    System_Config_t loaded;
    if (config == 0) return IMU_ERR_INVALID_PARAM;
    if (!flash_ready) return IMU_ERR_FLASH;
    spi_error = 0;
    if (read_config(&loaded) != IMU_OK) return IMU_ERR_FLASH;
    if (loaded.magic != IMU_CONFIG_MAGIC || loaded.crc32 != config_crc(&loaded)
        || !valid_config(&loaded)) return IMU_ERR_FLASH;
    *config = loaded;
    return IMU_OK;
}
