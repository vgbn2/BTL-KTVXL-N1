#ifndef IMU_TYPES_H
#define IMU_TYPES_H

#include <stdint.h>
#include <stdbool.h>


 


// Hang so he thong //

// Tran Khac Anh: Tan so ngat Timer lay mau (100 Hz) //
#define IMU_SAMPLE_RATE_HZ       100U

// Tran Khac Anh: Chu ky lay mau co dinh dt = 10ms //
#define IMU_SAMPLE_PERIOD_S      0.01f

// Nguyen Duy Anh: He so loc bu mac dinh //
#define IMU_DEFAULT_ALPHA        0.98f

// Tran Tuan Anh: Nguong alpha nho nhat cho lenh UART //
#define IMU_ALPHA_MIN            0.50f

// Tran Tuan Anh: Nguong alpha lon nhat cho lenh UART //
#define IMU_ALPHA_MAX            0.999f

// Tran Quoc An: So mau lay tinh de tinh offset //
#define IMU_CALIB_SAMPLES        200U

// Tran Tuan Anh: Ma nhan dien du lieu Flash hop le ("IMU4") //
#define IMU_CONFIG_MAGIC         0x494D5534UL 
 



 


// Ma trang thai tra ve chung //
typedef enum {
    
    // Ca nhom: Xu ly thanh cong
    IMU_OK = 0,
    
    // Tran Duc Anh: Loi giao tiep I2C voi MPU6050
    IMU_ERR_I2C = -1,
    
    // Tran Duc Anh: Sai ma chip WHO_AM_I
    IMU_ERR_DEVICE_ID = -2,
    
    // Tran Tuan Anh: Tham so alpha nhan qua UART khong hop le
    IMU_ERR_INVALID_PARAM = -3,
    
    // Tran Tuan Anh: Loi giao tiep SPI hoac hong chip Flash W25Qxx
    IMU_ERR_FLASH = -4,
    
    // Tran Quoc An: He thong chua duoc chay hieu chuan
    IMU_ERR_NOT_CALIBRATED = -5
} IMU_Status_t;


 


// Du lieu cam bien da doi sang don vi g va dps
typedef struct {
    
    // Tran Duc Anh doc ra, Tran Quoc An tru offset: Gia toc truc X (g)
    float ax_g;
    
    // Tran Duc Anh doc ra, Tran Quoc An tru offset: Gia toc truc Y (g)
    float ay_g;
    
    // Tran Duc Anh doc ra, Tran Quoc An tru offset: Gia toc truc Z (g)
    float az_g;
    
    // Tran Duc Anh doc ra, Tran Quoc An tru offset: Van toc goc truc X (dps)
    float gx_dps;
    
    // Tran Duc Anh doc ra, Tran Quoc An tru offset: Van toc goc truc Y (dps)
    float gy_dps;
    
    // Tran Duc Anh doc ra, Tran Quoc An tru offset: Van toc goc truc Z (dps)
    float gz_dps;
} IMU_Data_t;


 


// Ket qua goc nghieng dau ra
typedef struct {
    
    // Nguyen Duy Anh tinh, Tran Khac Anh ve len TFT, Tran Tuan Anh gui UART: Goc Roll
    float roll_deg;
    
    // Nguyen Duy Anh tinh, Tran Khac Anh ve len TFT, Tran Tuan Anh gui UART: Goc Pitch
    float pitch_deg;
} Angle_Data_t;


 


// Bo so sai so tinh cua cam bien
typedef struct {
    
    // Tran Quoc An tinh ra, Tran Tuan Anh luu Flash: Offset gia toc truc X
    float accel_offset_x;
    
    // Tran Quoc An tinh ra, Tran Tuan Anh luu Flash: Offset gia toc truc Y
    float accel_offset_y;
    
    // Tran Quoc An tinh ra, Tran Tuan Anh luu Flash: Offset gia toc truc Z (da tru 1g)
    float accel_offset_z;
    
    // Tran Quoc An tinh ra, Tran Tuan Anh luu Flash: Offset con quay truc X
    float gyro_offset_x;
    
    // Tran Quoc An tinh ra, Tran Tuan Anh luu Flash: Offset con quay truc Y
    float gyro_offset_y;
    
    // Tran Quoc An tinh ra, Tran Tuan Anh luu Flash: Offset con quay truc Z
    float gyro_offset_z;
} IMU_Offset_t;

 

// Cau truc luu tru cau hinh trong Flash W25Qxx
typedef struct {
    
    // Tran Tuan Anh: Ma kiem tra 0x494D5534 xac dinh Flash co du lieu
    uint32_t magic;        
 

    
    // Tran Tuan Anh luu xuong Flash, doc len nap lai cho Tran Quoc An
    IMU_Offset_t offset;   
 

    
    // Tran Tuan Anh luu xuong Flash, doc len nap lai cho Nguyen Duy Anh
    float alpha;           
 

    
    // Tran Tuan Anh: Ma kiem tra toan ven du lieu
    uint32_t crc32;        
 

} System_Config_t;


// IMU_TYPES_H
#endif 
