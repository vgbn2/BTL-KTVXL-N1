#include "display_system.h"
#include "system_time.h"

// Dieu khien man hinh TFT
#include "ili9341.h"

// Doc gia toc va van toc goc
#include "mpu6050.h"
#include "calibration.h"

// Ket hop du lieu de tinh goc
#include "angle_filter.h"
#include <stdio.h>
#include <string.h>

// thoi diem su ly du lieu
static volatile uint8_t timer_flag;
static uint32_t last_sample_ms, last_display_ms;

// Trang thai ban dau
static SystemState state = SYSTEM_INIT;
static bool has_angle = false;
static int lcd_ok = 0;
static bool sensor_ready = false;
static uint32_t retry_ms;
void DisplaySystem_SetSensorReady(bool ready) { sensor_ready = ready; }

// goc
static float roll, pitch;
static char lines[4][25];
static uint32_t draw_index = 96U;

// Ham cho cac module khac thay doi trang thai
void DisplaySystem_SetState(SystemState new_state) { state = new_state; }

// Ham cap nhat goc neu module khac can su dung
void DisplaySystem_SetAngle(float new_roll, float new_pitch)
{ roll = new_roll; pitch = new_pitch; }
bool DisplaySystem_HasAngle(void)
{ return has_angle && state != SYSTEM_ERROR; }
void DisplaySystem_Resync(void)
{
    uint32_t mask = __get_PRIMASK();
    __disable_irq(); timer_flag = 0; __set_PRIMASK(mask);
    last_sample_ms = SystemTime_GetMillis();
    Angle_Init(Angle_GetAlpha());
    has_angle = false;
}

// khoi tao man hinh TFT
void DisplaySystem_Init(void)
{
    lcd_ok = ILI9341_Init() == 0;
    
    // To mau nen den
    if (lcd_ok) lcd_ok = ILI9341_FillScreen(ILI9341_BLACK) == 0;
    state = SYSTEM_INIT; draw_index = 96U;
    last_display_ms = SystemTime_GetMillis();
}

// cau hinh TIMER2 , ngat sau 10 ms
void DisplaySystem_TimerInit(void)
{
    RCC_ClocksTypeDef clocks;
    TIM_TimeBaseInitTypeDef tim;
    NVIC_InitTypeDef nvic;
    uint32_t timer_clock;
    RCC_GetClocksFreq(&clocks);
    timer_clock = clocks.PCLK1_Frequency;
    if (clocks.PCLK1_Frequency != clocks.HCLK_Frequency) timer_clock *= 2U;
    
    // bat clock cho TIMER2 , nam tren bus APB1 cua STM32F103
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM2, ENABLE);
    TIM_TimeBaseStructInit(&tim);
    tim.TIM_Prescaler = (uint16_t)(timer_clock / 10000U - 1U);
    tim.TIM_Period = 99U;
    TIM_TimeBaseInit(TIM2, &tim);
    TIM_ClearITPendingBit(TIM2, TIM_IT_Update);
    TIM_ITConfig(TIM2, TIM_IT_Update, ENABLE);
    
    // cho phep NVIC quan ly ngat TIM2
    nvic.NVIC_IRQChannel = TIM2_IRQn;
    nvic.NVIC_IRQChannelPreemptionPriority = 1;
    nvic.NVIC_IRQChannelSubPriority = 0;
    nvic.NVIC_IRQChannelCmd = ENABLE; NVIC_Init(&nvic);
    last_sample_ms = SystemTime_GetMillis();
    
    // Bat dau dem TIMER2
    TIM_Cmd(TIM2, ENABLE);
}

// ham ngat TIMER2
void TIM2_IRQHandler(void)
{
    if (TIM_GetITStatus(TIM2, TIM_IT_Update) != RESET)
    
    // Xoa co ngat de TIMER tiep tuc hoat dong
    { TIM_ClearITPendingBit(TIM2, TIM_IT_Update); timer_flag = 1; }
}

// giao dien TFT
static void prepare_screen(void)
{
    const char *status = "INIT";
    uint32_t row, length;
    if (state == SYSTEM_RUNNING) status = Calibration_IsCalibrated() ? "RUNNING" : "UNCALIBRATED";
    if (state == SYSTEM_READY) status = "READY";
    if (state == SYSTEM_CALIBRATING) status = "CALIBRATING";
    if (state == SYSTEM_ERROR) status = "SENSOR ERROR";
    
    // Nhan Roll
    snprintf(lines[0], 25, "ROLL: %7.2f", (double)roll);
    
    // Chuyen goc Pitch thanh chuoi de hien thi
    snprintf(lines[1], 25, "PITCH:%7.2f", (double)pitch);
    snprintf(lines[2], 25, "ALPHA: %.3f", (double)Angle_GetAlpha());
    snprintf(lines[3], 25, "%s", status);
    for (row = 0; row < 4; row++)
    { length = strlen(lines[row]); while (length < 24U) lines[row][length++] = ' '; lines[row][24] = 0; }
    draw_index = 0;
}

// xu ly chinh cua module
void DisplaySystem_Process(void)
{
    IMU_Data_t data;
    Angle_Data_t angle;
    uint32_t now, elapsed, mask;
    if (!sensor_ready && (uint32_t)(SystemTime_GetMillis() - retry_ms) >= 1000U)
    {
        retry_ms = SystemTime_GetMillis();
        sensor_ready = MPU6050_Init() == IMU_OK;
        DisplaySystem_Resync();
    }
    if (timer_flag)
    {
        mask = __get_PRIMASK();
        __disable_irq(); timer_flag = 0; __set_PRIMASK(mask);
        now = SystemTime_GetMillis(); elapsed = now - last_sample_ms; last_sample_ms = now;
        if (!sensor_ready || MPU6050_Read(&data) != IMU_OK)
        { state = SYSTEM_ERROR; has_angle = false; sensor_ready = false; }
        else
        {
            if (!has_angle || elapsed > 50U) Angle_Init(Angle_GetAlpha());
            Calibration_Apply(&data);
            
            // Tinh Roll va Pitch bang Complementary Filter
            Angle_Update(&data, elapsed > 0U && elapsed <= 50U ? elapsed / 1000.0f : IMU_SAMPLE_PERIOD_S);
            Angle_Get(&angle); DisplaySystem_SetAngle(angle.roll_deg, angle.pitch_deg);
            
            // Cam bien doc duoc du lieu, he thong dang chay
            has_angle = true; state = SYSTEM_RUNNING;
        }
    }
     
    now = SystemTime_GetMillis();
    if (draw_index >= 96U && (uint32_t)(now - last_display_ms) >= 100U)
    
    // Ve lai gia tri goc va trang thai
    { last_display_ms = now; prepare_screen(); }
    if (lcd_ok && draw_index < 96U)
    {
        lcd_ok = ILI9341_DrawChar(10U + (draw_index % 24U) * 12U,
            30U + (draw_index / 24U) * 45U, lines[draw_index / 24U][draw_index % 24U],
            state == SYSTEM_ERROR ? ILI9341_RED : ILI9341_WHITE) == 0;
        draw_index++;
    }
}
