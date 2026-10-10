#include "w25qxx.h"
#include <string.h>

#if defined(STM32F103xB) || defined(USE_HAL_DRIVER)
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

static void spi_transmit(const uint8_t *data, uint16_t len) {
    HAL_SPI_Transmit(&hspi1, (uint8_t *)data, len, 100);
}

static void spi_receive(uint8_t *data, uint16_t len) {
    HAL_SPI_Receive(&hspi1, data, len, 100);
}

static void delay_ms(uint32_t ms) {
    HAL_Delay(ms);
}
#else
/* Host / Simulator in-memory mock flash (64 KB) */
static uint8_t mock_flash_mem[64 * 1024];
static bool mock_initialized = false;

static void mock_init_if_needed(void) {
    if (!mock_initialized) {
        memset(mock_flash_mem, 0xFF, sizeof(mock_flash_mem));
        mock_initialized = true;
    }
}
#endif

#if defined(STM32F103xB) || defined(USE_HAL_DRIVER)
static void w25qxx_wait_busy(void) {
    uint8_t status = 0;
    do {
        cs_low();
        spi_transfer(W25QXX_CMD_READ_STATUS_R1);
        status = spi_transfer(0xFF);
        cs_high();
    } while (status & W25QXX_STATUS_BUSY);
}

static void w25qxx_write_enable(void) {
    cs_low();
    spi_transfer(W25QXX_CMD_WRITE_ENABLE);
    cs_high();
}
#endif

bool w25qxx_init(void) {
#if defined(STM32F103xB) || defined(USE_HAL_DRIVER)
    cs_high();
    delay_ms(10);
    uint32_t jedec = w25qxx_read_jedec_id();
    /* Winbond manufacturer ID is 0xEF, 64M-bit capacity is 0x17 */
    return ((jedec & 0xFF0000ul) == 0xEF0000ul);
#else
    mock_init_if_needed();
    return true;
#endif
}

uint32_t w25qxx_read_jedec_id(void) {
#if defined(STM32F103xB) || defined(USE_HAL_DRIVER)
    cs_low();
    spi_transfer(W25QXX_CMD_JEDEC_ID);
    uint8_t mfg = spi_transfer(0xFF);
    uint8_t mem_type = spi_transfer(0xFF);
    uint8_t cap = spi_transfer(0xFF);
    cs_high();
    return ((uint32_t)mfg << 16) | ((uint32_t)mem_type << 8) | cap;
#else
    return W25Q64_JEDEC_ID;
#endif
}

bool w25qxx_erase_sector(uint32_t sector_addr) {
#if defined(STM32F103xB) || defined(USE_HAL_DRIVER)
    w25qxx_write_enable();
    cs_low();
    spi_transfer(W25QXX_CMD_SECTOR_ERASE_4K);
    spi_transfer((uint8_t)((sector_addr >> 16) & 0xFF));
    spi_transfer((uint8_t)((sector_addr >> 8) & 0xFF));
    spi_transfer((uint8_t)(sector_addr & 0xFF));
    cs_high();
    w25qxx_wait_busy();
    return true;
#else
    mock_init_if_needed();
    uint32_t base = sector_addr & ~(FLASH_SECTOR_SIZE - 1);
    if (base + FLASH_SECTOR_SIZE <= sizeof(mock_flash_mem)) {
        memset(&mock_flash_mem[base], 0xFF, FLASH_SECTOR_SIZE);
        return true;
    }
    return false;
#endif
}

bool w25qxx_write_page(uint32_t page_addr, const uint8_t *data, uint16_t len) {
    if (!data || len == 0 || len > FLASH_PAGE_SIZE) return false;

#if defined(STM32F103xB) || defined(USE_HAL_DRIVER)
    w25qxx_write_enable();
    cs_low();
    spi_transfer(W25QXX_CMD_PAGE_PROGRAM);
    spi_transfer((uint8_t)((page_addr >> 16) & 0xFF));
    spi_transfer((uint8_t)((page_addr >> 8) & 0xFF));
    spi_transfer((uint8_t)(page_addr & 0xFF));
    spi_transmit(data, len);
    cs_high();
    w25qxx_wait_busy();
    return true;
#else
    mock_init_if_needed();
    if (page_addr + len <= sizeof(mock_flash_mem)) {
        for (uint16_t i = 0; i < len; i++) {
            mock_flash_mem[page_addr + i] &= data[i]; /* Flash bit programming behavior */
        }
        return true;
    }
    return false;
#endif
}

bool w25qxx_read_data(uint32_t addr, uint8_t *buf, uint32_t len) {
    if (!buf || len == 0) return false;

#if defined(STM32F103xB) || defined(USE_HAL_DRIVER)
    cs_low();
    spi_transfer(W25QXX_CMD_READ_DATA);
    spi_transfer((uint8_t)((addr >> 16) & 0xFF));
    spi_transfer((uint8_t)((addr >> 8) & 0xFF));
    spi_transfer((uint8_t)(addr & 0xFF));
    spi_receive(buf, len);
    cs_high();
    return true;
#else
    mock_init_if_needed();
    if (addr + len <= sizeof(mock_flash_mem)) {
        memcpy(buf, &mock_flash_mem[addr], len);
        return true;
    }
    return false;
#endif
}

bool w25qxx_save_config(const flash_config_block_t *cfg) {
    if (!cfg) return false;

    flash_config_block_t sealed_cfg = *cfg;
    storage_seal_config(&sealed_cfg);

    /* Erase Sector 0 */
    if (!w25qxx_erase_sector(FLASH_CONFIG_SECTOR_ADDR)) {
        return false;
    }

    /* Write configuration to Page 0 of Sector 0 */
    return w25qxx_write_page(FLASH_CONFIG_SECTOR_ADDR,
                             (const uint8_t *)&sealed_cfg,
                             sizeof(flash_config_block_t));
}

bool w25qxx_load_config(flash_config_block_t *cfg) {
    if (!cfg) return false;

    if (!w25qxx_read_data(FLASH_CONFIG_SECTOR_ADDR, (uint8_t *)cfg, sizeof(flash_config_block_t))) {
        return false;
    }

    return storage_validate_config(cfg);
}
