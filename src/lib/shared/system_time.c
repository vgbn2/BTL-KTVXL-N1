#include "system_time.h"
static volatile uint32_t milliseconds = 0;
void SystemTime_Init(void)
{
    SystemCoreClockUpdate();
    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);
    // DWT dem chu ky CPU de tao tre ngan cho I2C mem.
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
    SysTick_Config(SystemCoreClock / 1000U);
}
void SysTick_Handler(void) { milliseconds++; }
uint32_t SystemTime_GetMillis(void) { return milliseconds; }
void Delay_ms(uint32_t time_ms)
{
    uint32_t start = milliseconds;
    while ((uint32_t)(milliseconds - start) < time_ms) { }
}
void Delay_us(uint32_t time_us)
{
    uint32_t start = DWT->CYCCNT;
    uint32_t cycles = (SystemCoreClock / 1000000U) * time_us;
    while ((uint32_t)(DWT->CYCCNT - start) < cycles) { }
}
