#ifndef CALIBRATION_H
#define CALIBRATION_H
#include "imu_types.h"

// Lay 200 mau tinh, tinh bu offset 6 truc MPU va tra ve ma loi
int Calibration_Run(void);

// tru truc tiep 6 gia tri offset vao du lieu cua moi chu ki do
void Calibration_Apply(IMU_Data_t *data);

// getter de lay cac offdet ra Flash
void Calibration_GetOffset(IMU_Offset_t *offset);

// setter nap offset tu Flash vao RAM
void Calibration_SetOffset(const IMU_Offset_t *offset);

// check hieu chuan thong so
bool Calibration_IsCalibrated(void);
#endif
