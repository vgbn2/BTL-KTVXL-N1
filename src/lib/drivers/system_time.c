#include "system_time.h"

#if defined(USE_STDPERIPH_DRIVER) || defined(STM32F10X)

static volatile uint32_t milliseconds = 0;

void SystemTime_Init(void)
{
    SystemCoreClockUpdate();
    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);
    /* DWT CPU cycle counter for microsecond delays in bit-banged I2C */
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
    SysTick_Config(SystemCoreClock / 1000U);
}

void SysTick_Handler(void)
{
    milliseconds++;
}

uint32_t SystemTime_GetMillis(void)
{
    return milliseconds;
}

#if defined(__GNUC__)
uint32_t __get_PRIMASK(void)
{
    uint32_t result;
    __asm volatile ("MRS %0, primask" : "=r" (result));
    return result;
}

void __set_PRIMASK(uint32_t priMask)
{
    __asm volatile ("MSR primask, %0" : : "r" (priMask) : "memory");
}
#endif

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

#else

/* Host simulation stubs */
static uint32_t host_sim_millis = 0;

void SystemTime_Init(void)
{
    host_sim_millis = 0;
}

uint32_t SystemTime_GetMillis(void)
{
    return ++host_sim_millis;
}

void Delay_ms(uint32_t time_ms)
{
    host_sim_millis += time_ms;
}

void Delay_us(uint32_t time_us)
{
    (void)time_us;
}

#endif
