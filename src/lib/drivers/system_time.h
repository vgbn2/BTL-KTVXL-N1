#ifndef SYSTEM_TIME_H
#define SYSTEM_TIME_H

#include <stdint.h>
#include <stdbool.h>

#if defined(USE_STDPERIPH_DRIVER) || defined(STM32F10X)
#include "stm32f10x.h"

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
#endif

/*
 * Pinout & Logic Level Configuration: 3.3V
 * MPU6050:     PB6 SCL, PB7 SDA, AD0 GND (4.7k pull-up to 3.3V)
 * W25Qxx SPI1: PA5 SCK, PA6 MISO, PA7 MOSI, PA4 CS
 * ILI9341 SPI2: PB13 SCK, PB15 MOSI, PB12 CS, PB10 DC, PB11 RESET
 * UART1:       PA9 TX, PA10 RX (3.3V TTL, common GND)
 * SWD:         PA13 SWDIO, PA14 SWCLK
 */
void SystemTime_Init(void);
uint32_t SystemTime_GetMillis(void);
void Delay_ms(uint32_t time_ms);
void Delay_us(uint32_t time_us);

#endif /* SYSTEM_TIME_H */
