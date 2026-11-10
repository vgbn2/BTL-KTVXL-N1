#ifndef MPU6050_H
#define MPU6050_H
#include "imu_types.h"
#define MPU6050_I2C_ADDR 0x68U

// init MPU6050, 100hz tan so quet,+-2g accel, +-250 gyro
int MPU6050_Init(void);

// su dung boi calibration , doc 14byte
int MPU6050_Read(IMU_Data_t *data);
bool MPU6050_CheckConnection(void);

// MPU6050_H
#endif
