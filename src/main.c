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
    
    // khoi tao UART
    UART_Init();
    
    // khoii tao bo loc
    Angle_Init(IMU_DEFAULT_ALPHA);
    
    // khoi tao TFT
    DisplaySystem_Init();
    
    // khoi tao MPU6050
    sensor_status = MPU6050_Init();
    
    // doc cau hinh Flash
    if (Flash_Init() == IMU_OK && Flash_LoadConfig(&config) == IMU_OK)
    { Calibration_SetOffset(&config.offset); Angle_Init(config.alpha); }
    DisplaySystem_SetSensorReady(sensor_status == IMU_OK);
    DisplaySystem_SetState(sensor_status == IMU_OK ? SYSTEM_READY : SYSTEM_ERROR);
    
    // khoi dong Timer2
    DisplaySystem_TimerInit();
    while (1)
    {
        
        // doc MPU6050, tinh Roll/Pitch
        DisplaySystem_Process();
        
        // kiem tra lenh UART tu may tinh
        UART_Process();
    }
}
