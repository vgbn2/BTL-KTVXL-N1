#include "w25qxx.h"
#include "flash_config.h"
#include <string.h>

#if defined(USE_STDPERIPH_DRIVER) || defined(STM32F10X)
#include "stm32f10x.h"

/* ===== Cau hinh chan SPI1 cho W25Qxx (SPL) ===== */
#define W25Q_SPI          SPI1
#define W25Q_CS_PORT      GPIOA
#define W25Q_CS_PIN       GPIO_Pin_4

#define W25Q_CS_LOW()     GPIO_ResetBits(W25Q_CS_PORT, W25Q_CS_PIN)
#define W25Q_CS_HIGH()    GPIO_SetBits(W25Q_CS_PORT, W25Q_CS_PIN)

static uint8_t spi_transfer(uint8_t tx_data)
{
    while (SPI_I2S_GetFlagStatus(W25Q_SPI, SPI_I2S_FLAG_TXE) == RESET);
    SPI_I2S_SendData(W25Q_SPI, tx_data);
    while (SPI_I2S_GetFlagStatus(W25Q_SPI, SPI_I2S_FLAG_RXNE) == RESET);
    return SPI_I2S_ReceiveData(W25Q_SPI);
}

static void w25q_wait_busy(void)
{
    uint8_t status;
    uint32_t timeout = 500000; /* Protect against infinite loop on disconnected bus */
    W25Q_CS_LOW();
    spi_transfer(W25Q_CMD_READ_STATUS1);
    do {
        status = spi_transfer(0xFF);
    } while ((status & W25QXX_STATUS_BUSY) && --timeout > 0);
    W25Q_CS_HIGH();
}

static void w25q_write_enable(void)
{
    W25Q_CS_LOW();
    spi_transfer(W25Q_CMD_WRITE_ENABLE);
    W25Q_CS_HIGH();
}

static void w25q_sector_erase(uint32_t addr)
{
    w25q_write_enable();
    W25Q_CS_LOW();
    spi_transfer(W25Q_CMD_SECTOR_ERASE);
    spi_transfer((addr >> 16) & 0xFF);
    spi_transfer((addr >> 8)  & 0xFF);
    spi_transfer(addr & 0xFF);
    W25Q_CS_HIGH();
    w25q_wait_busy();
}

static void w25q_page_program(uint32_t addr, const uint8_t *data, uint16_t len)
{
    w25q_write_enable();
    W25Q_CS_LOW();
    spi_transfer(W25Q_CMD_PAGE_PROGRAM);
    spi_transfer((addr >> 16) & 0xFF);
    spi_transfer((addr >> 8)  & 0xFF);
    spi_transfer(addr & 0xFF);
    for (uint16_t i = 0; i < len; i++) {
        spi_transfer(data[i]);
    }
    W25Q_CS_HIGH();
    w25q_wait_busy();
}

static void w25q_read_data(uint32_t addr, uint8_t *buf, uint16_t len)
{
    W25Q_CS_LOW();
    spi_transfer(W25Q_CMD_READ_DATA);
    spi_transfer((addr >> 16) & 0xFF);
    spi_transfer((addr >> 8)  & 0xFF);
    spi_transfer(addr & 0xFF);
    for (uint16_t i = 0; i < len; i++) {
        buf[i] = spi_transfer(0xFF);
    }
    W25Q_CS_HIGH();
}

__attribute__((weak)) int Flash_Init(void)
{
    GPIO_InitTypeDef gpio;
    SPI_InitTypeDef  spi;
    uint8_t id[3];

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_SPI1, ENABLE);

    /* PA5 = SCK, PA7 = MOSI: AF_PP */
    gpio.GPIO_Pin   = GPIO_Pin_5 | GPIO_Pin_7;
    gpio.GPIO_Mode  = GPIO_Mode_AF_PP;
    gpio.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &gpio);

    /* PA6 = MISO: IN_FLOATING */
    gpio.GPIO_Pin  = GPIO_Pin_6;
    gpio.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(GPIOA, &gpio);

    /* PA4 = CS: Out_PP */
    gpio.GPIO_Pin   = W25Q_CS_PIN;
    gpio.GPIO_Mode  = GPIO_Mode_Out_PP;
    gpio.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(W25Q_CS_PORT, &gpio);
    W25Q_CS_HIGH();

    /* SPI1 master, mode 0, 8-bit, prescaler 4 (~18 MHz) */
    spi.SPI_Direction         = SPI_Direction_2Lines_FullDuplex;
    spi.SPI_Mode              = SPI_Mode_Master;
    spi.SPI_DataSize          = SPI_DataSize_8b;
    spi.SPI_CPOL              = SPI_CPOL_Low;
    spi.SPI_CPHA              = SPI_CPHA_1Edge;
    spi.SPI_NSS               = SPI_NSS_Soft;
    spi.SPI_BaudRatePrescaler = SPI_BaudRatePrescaler_4;
    spi.SPI_FirstBit          = SPI_FirstBit_MSB;
    spi.SPI_CRCPolynomial     = 7;
    SPI_Init(W25Q_SPI, &spi);
    SPI_Cmd(W25Q_SPI, ENABLE);

    /* Doc JEDEC ID: W25Q64 -> 0xEF4017 */
    W25Q_CS_LOW();
    spi_transfer(W25Q_CMD_JEDEC_ID);
    id[0] = spi_transfer(0xFF);
    id[1] = spi_transfer(0xFF);
    id[2] = spi_transfer(0xFF);
    W25Q_CS_HIGH();

    if (id[0] != 0xEF) {
        return IMU_ERR_FLASH;
    }
    return IMU_OK;
}

bool w25qxx_init(void)
{
    return (Flash_Init() == IMU_OK);
}

uint32_t w25qxx_read_jedec_id(void)
{
    uint8_t id[3] = {0};
    W25Q_CS_LOW();
    spi_transfer(W25Q_CMD_JEDEC_ID);
    id[0] = spi_transfer(0xFF);
    id[1] = spi_transfer(0xFF);
    id[2] = spi_transfer(0xFF);
    W25Q_CS_HIGH();
    return ((uint32_t)id[0] << 16) | ((uint32_t)id[1] << 8) | id[2];
}

void w25qxx_set_mock_jedec_id(uint8_t mfg, uint8_t type, uint8_t cap)
{
    (void)mfg; (void)type; (void)cap;
}

bool w25qxx_erase_sector(uint32_t sector_addr)
{
    w25q_sector_erase(sector_addr);
    return true;
}

bool w25qxx_write_page(uint32_t page_addr, const uint8_t *data, uint16_t len)
{
    if (!data || len == 0 || len > FLASH_PAGE_SIZE) return false;
    /* Winbond W25Q64 Page Boundary Wrap Protection: address offset + len must not exceed 256 bytes */
    if (((page_addr & 0xFF) + len) > FLASH_PAGE_SIZE) return false;
    w25q_page_program(page_addr, data, len);
    return true;
}

bool w25qxx_read_data(uint32_t addr, uint8_t *buf, uint16_t len)
{
    if (!buf || len == 0) return false;
    w25q_read_data(addr, buf, len);
    return true;
}

#elif defined(STM32F103xB) || defined(USE_HAL_DRIVER)
#include "stm32f1xx_hal.h"
extern SPI_HandleTypeDef hspi1;

#define CS_PORT GPIOA
#define CS_PIN  GPIO_PIN_4

static void cs_low(void) {
    HAL_GPIO_WritePin(CS_PORT, CS_PIN, GPIO_PIN_RESET);
}

static void cs_high(void) {
    HAL_GPIO_WritePin(CS_PORT, CS_PIN, GPIO_PIN_SET);
}

static uint8_t spi_transfer(uint8_t tx_data) {
    uint8_t rx_data = 0;
    HAL_SPI_TransmitReceive(&hspi1, &tx_data, &rx_data, 1, 100);
    return rx_data;
}

static void w25q_wait_busy(void) {
    uint8_t status = 0;
    uint32_t timeout = 500000;
    cs_low();
    spi_transfer(W25QXX_CMD_READ_STATUS_R1);
    do {
        status = spi_transfer(0xFF);
    } while ((status & W25QXX_STATUS_BUSY) && --timeout > 0);
    cs_high();
}

static void w25q_write_enable(void) {
    cs_low();
    spi_transfer(W25QXX_CMD_WRITE_ENABLE);
    cs_high();
}

static void w25q_sector_erase(uint32_t addr) {
    w25q_write_enable();
    cs_low();
    spi_transfer(W25QXX_CMD_SECTOR_ERASE_4K);
    spi_transfer((addr >> 16) & 0xFF);
    spi_transfer((addr >> 8) & 0xFF);
    spi_transfer(addr & 0xFF);
    cs_high();
    w25q_wait_busy();
}

static void w25q_page_program(uint32_t addr, const uint8_t *data, uint16_t len) {
    w25q_write_enable();
    cs_low();
    spi_transfer(W25QXX_CMD_PAGE_PROGRAM);
    spi_transfer((addr >> 16) & 0xFF);
    spi_transfer((addr >> 8) & 0xFF);
    spi_transfer(addr & 0xFF);
    for (uint16_t i = 0; i < len; i++) {
        spi_transfer(data[i]);
    }
    cs_high();
    w25q_wait_busy();
}

static void w25q_read_data(uint32_t addr, uint8_t *buf, uint16_t len) {
    cs_low();
    spi_transfer(W25QXX_CMD_READ_DATA);
    spi_transfer((addr >> 16) & 0xFF);
    spi_transfer((addr >> 8) & 0xFF);
    spi_transfer(addr & 0xFF);
    for (uint16_t i = 0; i < len; i++) {
        buf[i] = spi_transfer(0xFF);
    }
    cs_high();
}

__attribute__((weak)) int Flash_Init(void) {
    uint32_t jedec = w25qxx_read_jedec_id();
    if ((jedec >> 16) != 0xEF) {
        return IMU_ERR_FLASH;
    }
    return IMU_OK;
}

bool w25qxx_init(void) {
    return (Flash_Init() == IMU_OK);
}

uint32_t w25qxx_read_jedec_id(void) {
    cs_low();
    spi_transfer(W25QXX_CMD_JEDEC_ID);
    uint32_t id = 0;
    id |= ((uint32_t)spi_transfer(0xFF)) << 16;
    id |= ((uint32_t)spi_transfer(0xFF)) << 8;
    id |= ((uint32_t)spi_transfer(0xFF));
    cs_high();
    return id;
}

void w25qxx_set_mock_jedec_id(uint8_t mfg, uint8_t type, uint8_t cap) {
    (void)mfg; (void)type; (void)cap;
}

bool w25qxx_erase_sector(uint32_t sector_addr) {
    w25q_sector_erase(sector_addr);
    return true;
}

bool w25qxx_write_page(uint32_t page_addr, const uint8_t *data, uint16_t len) {
    if (!data || len == 0 || len > FLASH_PAGE_SIZE) return false;
    /* Winbond W25Q64 Page Boundary Wrap Protection: address offset + len must not exceed 256 bytes */
    if (((page_addr & 0xFF) + len) > FLASH_PAGE_SIZE) return false;
    w25q_page_program(page_addr, data, len);
    return true;
}

bool w25qxx_read_data(uint32_t addr, uint8_t *buf, uint16_t len) {
    if (!buf || len == 0) return false;
    w25q_read_data(addr, buf, len);
    return true;
}

#else
/* Host / Simulator in-memory mock flash (64 KB) */
static uint8_t mock_flash_mem[64 * 1024];
static bool mock_initialized = false;
static uint8_t mock_jedec_bytes[3] = {0xEF, 0x40, 0x17}; /* Winbond W25Q64 */

void w25qxx_set_mock_jedec_id(uint8_t mfg, uint8_t type, uint8_t cap) {
    mock_jedec_bytes[0] = mfg;
    mock_jedec_bytes[1] = type;
    mock_jedec_bytes[2] = cap;
}

static void mock_init_if_needed(void) {
    if (!mock_initialized) {
        memset(mock_flash_mem, 0xFF, sizeof(mock_flash_mem));
        mock_initialized = true;
    }
}

static void w25q_sector_erase(uint32_t addr) {
    mock_init_if_needed();
    uint32_t base = addr & ~(FLASH_SECTOR_SIZE - 1);
    if (base + FLASH_SECTOR_SIZE <= sizeof(mock_flash_mem)) {
        memset(&mock_flash_mem[base], 0xFF, FLASH_SECTOR_SIZE);
    }
}

static void w25q_page_program(uint32_t addr, const uint8_t *data, uint16_t len) {
    mock_init_if_needed();
    if (addr + len <= sizeof(mock_flash_mem)) {
        for (uint16_t i = 0; i < len; i++) {
            mock_flash_mem[addr + i] &= data[i];
        }
    }
}

static void w25q_read_data(uint32_t addr, uint8_t *buf, uint16_t len) {
    mock_init_if_needed();
    if (addr + len <= sizeof(mock_flash_mem)) {
        memcpy(buf, &mock_flash_mem[addr], len);
    }
}

uint32_t w25qxx_read_jedec_id(void) {
    mock_init_if_needed();
    return ((uint32_t)mock_jedec_bytes[0] << 16) |
           ((uint32_t)mock_jedec_bytes[1] << 8)  |
           ((uint32_t)mock_jedec_bytes[2]);
}

__attribute__((weak)) int Flash_Init(void) {
    mock_init_if_needed();
    uint32_t jedec = w25qxx_read_jedec_id();
    if ((jedec >> 16) != 0xEF) {
        return IMU_ERR_FLASH;
    }
    return IMU_OK;
}

bool w25qxx_init(void) {
    return (Flash_Init() == IMU_OK);
}

bool w25qxx_erase_sector(uint32_t sector_addr) {
    mock_init_if_needed();
    uint32_t base = sector_addr & ~(FLASH_SECTOR_SIZE - 1);
    if (base + FLASH_SECTOR_SIZE <= sizeof(mock_flash_mem)) {
        memset(&mock_flash_mem[base], 0xFF, FLASH_SECTOR_SIZE);
        return true;
    }
    return false;
}

bool w25qxx_write_page(uint32_t page_addr, const uint8_t *data, uint16_t len) {
    if (!data || len == 0 || len > FLASH_PAGE_SIZE) return false;
    /* Winbond W25Q64 Page Boundary Wrap Protection: address offset + len must not exceed 256 bytes */
    if (((page_addr & 0xFF) + len) > FLASH_PAGE_SIZE) return false;
    mock_init_if_needed();
    if (page_addr + len <= sizeof(mock_flash_mem)) {
        for (uint16_t i = 0; i < len; i++) {
            mock_flash_mem[page_addr + i] &= data[i];
        }
        return true;
    }
    return false;
}

bool w25qxx_read_data(uint32_t addr, uint8_t *buf, uint16_t len) {
    if (!buf || len == 0) return false;
    mock_init_if_needed();
    if (addr + len <= sizeof(mock_flash_mem)) {
        memcpy(buf, &mock_flash_mem[addr], len);
        return true;
    }
    return false;
}
#endif

/* ===== APIs cho System_Config_t (TV4 Google Doc) ===== */
__attribute__((weak)) int Flash_SaveConfig(const System_Config_t *config)
{
    if (config == NULL) return IMU_ERR_INVALID_PARAM;

    System_Config_t tmp = *config;
    tmp.magic = IMU_CONFIG_MAGIC;

    /* Tinh checksum XOR (don gian, du dung cho do an) */
    const uint8_t *p = (const uint8_t *)&tmp;
    uint32_t crc = 0;
    for (uint32_t i = 0; i < sizeof(System_Config_t) - sizeof(uint32_t); i++) {
        crc ^= p[i];
    }
    tmp.crc32 = crc;

    w25q_sector_erase(FLASH_CONFIG_BASE_ADDR);
    w25q_page_program(FLASH_CONFIG_BASE_ADDR, (const uint8_t *)&tmp, sizeof(System_Config_t));

    return IMU_OK;
}

__attribute__((weak)) int Flash_LoadConfig(System_Config_t *config)
{
    if (config == NULL) return IMU_ERR_INVALID_PARAM;

    System_Config_t tmp;
    w25q_read_data(FLASH_CONFIG_BASE_ADDR, (uint8_t *)&tmp, sizeof(System_Config_t));

    if (tmp.magic != IMU_CONFIG_MAGIC) return IMU_ERR_FLASH;

    const uint8_t *p = (const uint8_t *)&tmp;
    uint32_t crc = 0;
    for (uint32_t i = 0; i < sizeof(System_Config_t) - sizeof(uint32_t); i++) {
        crc ^= p[i];
    }
    if (crc != tmp.crc32) return IMU_ERR_FLASH;

    *config = tmp;
    return IMU_OK;
}

/* ===== APIs cho flash_config_block_t (Modular / Test suite) ===== */
bool w25qxx_save_config(const flash_config_block_t *cfg)
{
    if (!cfg) return false;

    flash_config_block_t sealed_cfg = *cfg;
    storage_seal_config(&sealed_cfg);

    if (!w25qxx_erase_sector(FLASH_CONFIG_SECTOR_ADDR)) {
        return false;
    }

    return w25qxx_write_page(FLASH_CONFIG_SECTOR_ADDR,
                             (const uint8_t *)&sealed_cfg,
                             sizeof(flash_config_block_t));
}

bool w25qxx_load_config(flash_config_block_t *cfg)
{
    if (!cfg) return false;

    if (!w25qxx_read_data(FLASH_CONFIG_SECTOR_ADDR, (uint8_t *)cfg, sizeof(flash_config_block_t))) {
        return false;
    }

    return storage_validate_config(cfg);
}
