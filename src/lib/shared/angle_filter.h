
#ifndef ANGLE_FILTER_H
#define ANGLE_FILTER_H

#include "imu_types.h"


 


// Khoi tao bo loc Complementary Filter
void Angle_Init(float alpha);


 


// Cap nhat goc Roll va Pitch tu du lieu MPU6050
void Angle_Update(const IMU_Data_t *data, float dt_s);


 


// Lay gia tri goc Roll va Pitch hien tai
void Angle_Get(Angle_Data_t *angle);


 


// Thay doi he so loc alpha
int Angle_SetAlpha(float alpha);


 


// Lay he so loc alpha hien tai
float Angle_GetAlpha(void);

#endif


