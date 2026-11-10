#ifndef SYSTEM_TIME_H
#define SYSTEM_TIME_H
#include "stm32f10x.h"
#include <stdint.h>

#ifndef DWT
typedef struct {
    volatile uint32_t CTRL;
    volatile uint32_t CYCCNT;
} DWT_Type;
#define DWT_BASE                (0xE0001000UL)
#define DWT                     ((DWT_Type *)DWT_BASE)
#define DWT_CTRL_CYCCNTENA_Msk  (1UL << 0)
#endif

#ifndef CoreDebug_DEMCR_TRCENA_Msk
#define CoreDebug_DEMCR_TRCENA_Msk (1UL << 24)
#endif

// So do chan de xuat, nguon va muc logic 3.3 V.
// MPU6050: PB6 SCL, PB7 SDA, AD0 GND. Can dien tro keo len 3.3 V.
// W25Qxx SPI1: PA5 SCK, PA6 MISO, PA7 MOSI, PA4 CS.
// ILI9341 SPI2: PB13 SCK, PB15 MOSI, PB12 CS, PB10 DC, PB11 RESET.
// LCD SDO khong dung. Chan LED cap theo thong so module, khong lay dong tu GPIO.
// UART1: PA9 TX, PA10 RX. USB-UART muc 3.3 V, chung GND.
// Giu PA13/PA14 cho ST-Link. Thach anh ngoai du kien 8 MHz.
void SystemTime_Init(void);
uint32_t SystemTime_GetMillis(void);
void Delay_ms(uint32_t time_ms);
void Delay_us(uint32_t time_us);
#endif
