#ifndef FLASH_CONFIG_H
#define FLASH_CONFIG_H

#include "imu_types.h"


 


// Dia chi luu cau hinh tren W25Qxx (can theo sector 4KB)
#define FLASH_CONFIG_BASE_ADDR   0x000000UL


 


// Lenh chuan W25Qxx
#define W25Q_CMD_WRITE_ENABLE    0x06
#define W25Q_CMD_READ_STATUS1    0x05
#define W25Q_CMD_READ_DATA       0x03
#define W25Q_CMD_PAGE_PROGRAM    0x02
#define W25Q_CMD_SECTOR_ERASE    0x20
#define W25Q_CMD_JEDEC_ID        0x9F

int Flash_Init(void);
int Flash_SaveConfig(const System_Config_t *config);
int Flash_LoadConfig(System_Config_t *config);


// FLASH_CONFIG_H
#endif 
 


