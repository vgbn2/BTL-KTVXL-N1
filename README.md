# BTL-KTVXL-N1
Bài tập lớn kĩ thuật vi xử lý của nhóm 1- D24-PTIT
Đề Tài : Thiết bị đo góc nghiêng sử dụng MPU6050

Accelerometer và Gyroscope của cảm biến MPU6050. Sử dụng
Complementary Filter để giảm nhiễu và sai số tích phân.
Linh kiện sử dụng:
MPU6050 (I2C)
Màn hình OLED/TFT
Flash W25Qxx
STM32 (I2C, SPI, UART, TIMER, FLASH)
Cài đặt thông số và phản hồi qua UART:
Hiệu chuẩn:
CALIBRATE_IMU
Cài đặt hệ số lọc:
SET_FILTER_ALPHA:0.98
Đọc góc:
GET_ANGLE
Phản hồi:
ROLL:12.4
PITCH:-5.8
Lưu offset và hệ số lọc vào FLASH.
Sử dụng TIMER để bảo đảm chu kỳ tính toán cố định.
