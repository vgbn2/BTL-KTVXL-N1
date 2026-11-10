#include "calibration.h"
#include "mpu6050.h"
#include <stddef.h>

#include "system_time.h"
 

// dong goi du lieu va khoi tao ban dau
static IMU_Offset_t current_offset={0};
static bool is_calibrated = false;


// Lay 200 mau tinh, tinh bu offset 6 truc MPU va tra ve ma loi
int Calibration_Run (void) {  
	
	// luu du lieu tho moi vong lap]
	IMU_Data_t data;  
	 
	
	// cong don gia tri 6 truc
	float sum_ax = 0.0f;
	float sum_ay = 0.0f;
	float sum_az =0.0f;
	
	float sum_gx = 0.0f;
	float sum_gy = 0.0f;
	float sum_gz = 0.0f;
	
	
	// nhan ma tra ve de kiem tra
	int status;  
	uint32_t i;
	
	// kiem tra loi trong qua trinh lay mau qua 200 vong lap
	for(i=0U;i<IMU_CALIB_SAMPLES;i++){  
		
		// nhan ma tu ham
		status = MPU6050_Read(&data); 
		
		// kiem tra xem dung chua
		if(status != IMU_OK) { 
			return status;
		}
		 
        if (!(data.ax_g >= -2.0f && data.ax_g <= 2.0f)
            || !(data.ay_g >= -2.0f && data.ay_g <= 2.0f)
            || !(data.az_g >= -2.0f && data.az_g <= 2.0f)
            || !(data.gx_dps >= -250.0f && data.gx_dps <= 250.0f)
            || !(data.gy_dps >= -250.0f && data.gy_dps <= 250.0f)
            || !(data.gz_dps >= -250.0f && data.gz_dps <= 250.0f)) return IMU_ERR_INVALID_PARAM;
         
	 
	 // lay tong mau do de chia trung binh cong
	 sum_ax = sum_ax + data.ax_g;
        sum_ay = sum_ay + data.ay_g;
        sum_az = sum_az + data.az_g;

        sum_gx = sum_gx + data.gx_dps;
        sum_gy = sum_gy + data.gy_dps;
        sum_gz = sum_gz + data.gz_dps;
		
		// delay 10ms trong 199m lan lay mau
		if(i<IMU_CALIB_SAMPLES-1U){  
			Delay_ms(10U);
		}
	}
	 
	
	// chia trung binh cong gia toc
	current_offset.accel_offset_x = sum_ax/IMU_CALIB_SAMPLES;
	current_offset.accel_offset_y = sum_ay/IMU_CALIB_SAMPLES;
	
	// tru truc z 1.0g
	current_offset.accel_offset_z = sum_az/IMU_CALIB_SAMPLES-1.0f; 
	 
	
	// chia trung binh cong van toc goc
	current_offset.gyro_offset_x = sum_gx/IMU_CALIB_SAMPLES;
	current_offset.gyro_offset_y = sum_gy/IMU_CALIB_SAMPLES;
	current_offset.gyro_offset_z = sum_gz/IMU_CALIB_SAMPLES;
	
	// tin offset da xong
	is_calibrated = true;  
	return IMU_OK;
}


// tru truc tiep 6 gia tri offset vao du lieu cua moi chu ki do
void Calibration_Apply(IMU_Data_t *data){ 
	 
	
	// kiem tra con tro rong va hieu chinh
	if(data==NULL) return;
	if(is_calibrated==false) return;
	 
	
	// lay du lieu thuc
	data->ax_g = data->ax_g - current_offset.accel_offset_x;
    data->ay_g = data->ay_g - current_offset.accel_offset_y;
    data->az_g = data->az_g - current_offset.accel_offset_z;

    data->gx_dps = data->gx_dps - current_offset.gyro_offset_x;
    data->gy_dps = data->gy_dps - current_offset.gyro_offset_y;
    data->gz_dps = data->gz_dps - current_offset.gyro_offset_z;
}

// getter de lay cac offdet ra Flash
void Calibration_GetOffset(IMU_Offset_t *offset){ 
	if (offset==NULL) return;
	
	// lay ban sao cua offset ra ngoai cho FLASH
	*offset = current_offset;  
}

// setter nap offset tu Flash vao RAM
void Calibration_SetOffset(const IMU_Offset_t *offset){ 
	if (offset==NULL) return;
	if (!(offset->accel_offset_x >= -2.0f && offset->accel_offset_x <= 2.0f)
        || !(offset->accel_offset_y >= -2.0f && offset->accel_offset_y <= 2.0f)
        || !(offset->accel_offset_z >= -3.0f && offset->accel_offset_z <= 1.0f)
        || !(offset->gyro_offset_x >= -250.0f && offset->gyro_offset_x <= 250.0f)
        || !(offset->gyro_offset_y >= -250.0f && offset->gyro_offset_y <= 250.0f)
        || !(offset->gyro_offset_z >= -250.0f && offset->gyro_offset_z <= 250.0f)) return;
    
    // nhan du lieu offset tu FLSAH
    current_offset=*offset;  
	is_calibrated=true;
}

// check hieu chuan thong so
bool Calibration_IsCalibrated(void){ 
	return is_calibrated;
}
