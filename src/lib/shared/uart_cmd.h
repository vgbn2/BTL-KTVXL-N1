#ifndef UART_CMD_H
#define UART_CMD_H

#include "imu_types.h"

void UART_Init(void);
void UART_Process(void);
void UART_SendAngle(const Angle_Data_t *angle);
void UART_SendGroupInfo(void);
void UART_SendBanner(void);


// UART_CMD_H
#endif 
 


