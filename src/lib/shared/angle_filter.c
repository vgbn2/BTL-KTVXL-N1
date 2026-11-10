
#include "angle_filter.h"
#include "imu_types.h"
#include <math.h>
#include <stddef.h>
#include <float.h>


 


// Chuyen doi tu radian sang do
#define ANGLE_RAD_TO_DEG 57.2957795f


 


// Bien noi bo cua module
static float roll_deg = 0.0f;
static float pitch_deg = 0.0f;
static float filter_alpha = IMU_DEFAULT_ALPHA;


 


// Danh dau da co goc khoi tao tu Accelerometer hay chua
static bool has_initial_angle = false;


 


// Khoi tao bo loc
void Angle_Init(float alpha)
{
    roll_deg = 0.0f;
    pitch_deg = 0.0f;

    has_initial_angle = false;

    if (alpha >= IMU_ALPHA_MIN && alpha <= IMU_ALPHA_MAX)
    {
        filter_alpha = alpha;
    }
    else
    {
        filter_alpha = IMU_DEFAULT_ALPHA;
    }
}


 


// Cap nhat goc Roll va Pitch
void Angle_Update(const IMU_Data_t *data, float dt_s)
{
    float acc_roll_deg;
    float acc_pitch_deg;
    float predicted_roll;
    float roll_error;

    
 

    
    // Kiem tra du lieu dau vao
    if ((data == NULL) || !(dt_s > 0.0f && dt_s <= 0.05f))
    {
        return;
    }

    if (!(data->ax_g >= -FLT_MAX && data->ax_g <= FLT_MAX)
        || !(data->ay_g >= -FLT_MAX && data->ay_g <= FLT_MAX)
        || !(data->az_g >= -FLT_MAX && data->az_g <= FLT_MAX)
        || !(data->gx_dps >= -250.0f && data->gx_dps <= 250.0f)
        || !(data->gy_dps >= -250.0f && data->gy_dps <= 250.0f)) return;
    if (data->ax_g * data->ax_g + data->ay_g * data->ay_g
        + data->az_g * data->az_g < 0.0001f) return;
     
    
    // Tinh goc Roll tu Accelerometer
    acc_roll_deg = atan2f(data->ay_g, data->az_g)
                   * ANGLE_RAD_TO_DEG;

    
 

    
    // Tinh goc Pitch tu Accelerometer
    acc_pitch_deg = atan2f(
                        -data->ax_g,
                        sqrtf(data->ay_g * data->ay_g
                            + data->az_g * data->az_g)
                    ) * ANGLE_RAD_TO_DEG;

    
 

    
    // Khoi tao goc ban dau tu Accelerometer
    if (!has_initial_angle)
    {
        roll_deg = acc_roll_deg;
        pitch_deg = acc_pitch_deg;

        has_initial_angle = true;
    }
    else
    {
        
 

         
        
        // Bo loc bu cho goc Roll
        predicted_roll = roll_deg + data->gx_dps * dt_s;
        roll_error = acc_roll_deg - predicted_roll;
        while (roll_error > 180.0f) roll_error -= 360.0f;
        while (roll_error < -180.0f) roll_error += 360.0f;
        roll_deg = predicted_roll + (1.0f - filter_alpha) * roll_error;
        while (roll_deg > 180.0f) roll_deg -= 360.0f;
        while (roll_deg < -180.0f) roll_deg += 360.0f;

        
 

        
        // Bo loc bu cho goc Pitch
        pitch_deg = filter_alpha
                    * (pitch_deg + data->gy_dps * dt_s)
                    + (1.0f - filter_alpha) * acc_pitch_deg;
    }
}


 


// Lay gia tri goc hien tai
void Angle_Get(Angle_Data_t *angle)
{
    if (angle == NULL)
    {
        return;
    }

    angle->roll_deg = roll_deg;
    angle->pitch_deg = pitch_deg;
}


 


// Thay doi he so loc alpha
int Angle_SetAlpha(float alpha)
{
    if (!(alpha >= IMU_ALPHA_MIN && alpha <= IMU_ALPHA_MAX))
    {
        return IMU_ERR_INVALID_PARAM;
    }

    filter_alpha = alpha;

    return IMU_OK;
}


 


// Lay he so loc alpha hien tai
float Angle_GetAlpha(void)
{
    return filter_alpha;
}


