#ifndef DISPLAY_SYSTEM_H
#define DISPLAY_SYSTEM_H
#include "imu_types.h"
typedef enum { 
// Dang khoi tao
SYSTEM_INIT, 
// Khoi tao thanh cong, san sang
SYSTEM_READY, 
// Dang doc cam bien va tinh goc
SYSTEM_RUNNING,
    
    // Dang hieu chuan cam bien
    SYSTEM_CALIBRATING, 
    // Xay ra loi
    SYSTEM_ERROR } SystemState;

// Khoi tao man hinh TFT va du lieu he thong.
void DisplaySystem_Init(void);

// Khoi tao TIMER2 de tao chu ky 10 ms
void DisplaySystem_TimerInit(void);

// Xu ly du lieu MPU6050 va cap nhat goc
void DisplaySystem_Process(void);

// Thay doi trang thai hien tai cua he thong
void DisplaySystem_SetState(SystemState state);

// Cap nhat gia tri goc Roll va Pitch
void DisplaySystem_SetAngle(float roll, float pitch);
void DisplaySystem_Resync(void);
bool DisplaySystem_HasAngle(void);
void DisplaySystem_SetSensorReady(bool ready);
#endif
