#include "system_time.h"
#include "display_system.h"
#include "mpu6050.h"
#include "calibration.h"
#include "angle_filter.h"
#include "uart_cmd.h"
#include "flash_config.h"

int main(void)
{
    System_Config_t config;
    int sensor_status;

    SystemTime_Init();

    /* Khoi tao UART */
    UART_Init();

    /* Khoi tao bo loc */
    Angle_Init(IMU_DEFAULT_ALPHA);

    /* Khoi tao TFT */
    DisplaySystem_Init();

    /* Khoi tao MPU6050 */
    sensor_status = MPU6050_Init();

    /* Doc cau hinh tu Flash */
    if (Flash_Init() == IMU_OK && Flash_LoadConfig(&config) == IMU_OK)
    {
        Calibration_SetOffset(&config.offset);
        Angle_Init(config.alpha);
    }

    DisplaySystem_SetSensorReady(sensor_status == IMU_OK);
    DisplaySystem_SetState((sensor_status == IMU_OK) ? SYSTEM_READY : SYSTEM_ERROR);

    /* Khoi dong Timer2 */
    DisplaySystem_TimerInit();

#if defined(USE_STDPERIPH_DRIVER) || defined(STM32F10X)
    while (1)
    {
        /* Doc MPU6050, tinh Roll/Pitch va ve man hinh */
        DisplaySystem_Process();

        /* Kiem tra va thuc thi lenh UART tu may tinh */
        UART_Process();
    }
#else
    /* Host simulation: run iterations to verify pipeline */
    for (int sim_step = 0; sim_step < 10; sim_step++)
    {
        DisplaySystem_Process();
        UART_Process();
    }
    return 0;
#endif
}
