# QUY TẮC CODE CHUNG – BÀI 4: THIẾT BỊ ĐO GÓC NGHIÊNG MPU6050

> **Đọc trước khi AI viết, sửa, giải thích hoặc ghép code.** Đây là quy chuẩn lập trình đã thống nhất của nhóm 5 thành viên cho Bài 4 (Vi xử lý 2026). Mọi thành viên và công cụ hỗ trợ AI phải tuân thủ đúng tên file, kiểu dữ liệu, nguyên mẫu hàm (prototype), quy ước trục và phân quyền module trong tài liệu này. Nếu phát hiện yêu cầu mới mâu thuẫn với tài liệu, hãy chỉ rõ xung đột và hỏi nhóm trước khi thay đổi API công khai.

## 1. Bối cảnh và yêu cầu đề tài

* **Vi điều khiển & Môi trường:** STM32 (dự kiến STM32F103C8T6 / STM32F4xx), lập trình trên **STM32CubeIDE** dùng thư viện **HAL** và **ngôn ngữ C** (`.c` + `.h`), không dùng `.cpp`.
* **Thuật toán cốt lõi:** Đọc dữ liệu gia tốc (Accelerometer) và vận tốc góc (Gyroscope) từ cảm biến **MPU6050** qua giao tiếp **I2C**; thực hiện hiệu chuẩn (Calibration) bù sai số tĩnh và tính góc **Roll / Pitch** bằng bộ lọc bù (**Complementary Filter**).
* **Ngoại vi & Lưu trữ:** Sử dụng **TIMER** để bảo đảm chu kỳ tính toán cố định (`dt_s`); hiển thị trực tiếp góc đo lên màn hình **OLED/TFT**; giao tiếp **UART** với máy tính; lưu trữ `offset` và hệ số lọc `alpha` vào bộ nhớ **Flash W25Qxx** (giao tiếp SPI) để khôi phục tự động sau khi mất nguồn.
* **Tập lệnh UART bắt buộc (gồm yêu cầu riêng Bài 4 + yêu cầu chung toàn khóa):**
  * `INFO_REQUEST` hoặc `INFO_REQUEST:GROUP`: phản hồi đầy đủ 6 trường thông tin nhóm (`GROUP_NAME`, `MEMBERS`, `PROJECT_NAME`, `HARDWARE_USED`, `SOFTWARE_USED`, `DATE`).
  * `CALIBRATE_IMU`: thực hiện lấy mẫu tĩnh, tính offset mới cho MPU6050 và lưu vào Flash.
  * `SET_FILTER_ALPHA:0.98`: cập nhật hệ số lọc Complementary Filter và lưu vào Flash.
  * `GET_ANGLE`: phản hồi góc hiện tại theo định dạng `ROLL:12.4` và `PITCH:-5.8`.
* **Yêu cầu đánh giá:** Cả 5 thành viên bắt buộc có phần code riêng, nắm vững luồng xử lý của module mình phụ trách và chạy thực tế trên phần cứng thật.

## 2. Quy tắc đặt tên và tổ chức mã nguồn

| Thành phần | Quy tắc | Ví dụ |
| :--- | :--- | :--- |
| File nguồn / header | chữ thường, `snake_case`, cùng tên gốc | `angle_filter.c`, `angle_filter.h` |
| Hàm công khai (Public API) | `Module_Action` | `MPU6050_Read`, `Calibration_Run` |
| Hàm nội bộ (Private helper) | `static` + `snake_case` | `static int w25qxx_wait_busy(void)` |
| Biến / tham số | `snake_case` | `gyro_offset_x`, `sample_flag` |
| Kiểu dữ liệu (`struct`, `enum`) | `PascalCase` + `_t` | `IMU_Data_t`, `IMU_Status_t` |
| Macro / hằng số | `UPPER_SNAKE_CASE` | `IMU_SAMPLE_RATE_HZ`, `CALIB_SAMPLES` |
| Biến trạng thái nội bộ module | `static` trong file `.c` | `static float roll_deg = 0.0f;` |
| Biến logic (cờ trạng thái) | tiền tố `is_`, `has_` | `is_calibrated`, `has_new_cmd` |
| Đơn vị vật lý | hậu tố rõ ràng | `_g`, `_dps`, `_deg`, `_s`, `_hz` |
| Include guard | tên file viết hoa + `_H` | `ANGLE_FILTER_H`, `CALIBRATION_H` |

* Không đặt tên biến mơ hồ (`a`, `temp1`, `val`) cho dữ liệu lưu trữ hoặc truyền giữa các hàm.
* Không định nghĩa biến toàn cục dùng chung trong file `.h`; mọi dữ liệu trao đổi giữa các module phải đi qua tham số hàm hoặc hàm `Get`/`Set`.
* Tuyệt đối không `#include` file `.c`; chỉ `#include` các file `.h` thực sự cần thiết.
* Không tự ý đổi tên hàm API, tên trường trong `struct`, đơn vị vật lý hay dấu góc.

## 3. Phân công thành viên, quyền sở hữu file và nhiệm vụ

Bảng dưới đây đồng bộ hóa bảng phân công nhân sự của nhóm với cấu trúc module chuẩn (tách riêng từng chức năng thành các cặp file `.c`/`.h` độc lập trong `Core/Inc/` và `Core/Src/`):

| Mã TV | Họ và tên | File sở hữu (`.c` & `.h`) | Trách nhiệm lập trình chính | Nhiệm vụ chung & Báo cáo (3 chương, ≥10 trang/chương) | Phần trình bày & Vấn đáp |
| :--- | :--- | :--- | :--- | :--- | :--- |
| **TV1** | **Trần Đức Anh** | `mpu6050.c`<br>`mpu6050.h` | Khởi tạo I2C & thanh ghi MPU6050, kiểm tra `WHO_AM_I`, đọc 14 byte Accel/Gyro thô, chuyển đổi sang đơn vị vật lý (`g`, `dps`). | Mua linh kiện, hỗ trợ lắp ráp phần cứng. | Giao tiếp I2C, bản đồ thanh ghi MPU6050, driver đọc cảm biến và demo dữ liệu thô/đổi đơn vị. |
| **TV2** | **Trần Quốc Ân** | `calibration.c`<br>`calibration.h` | Xử lý thuật toán `CALIBRATE_IMU`: lấy trung bình $N$ mẫu tĩnh, tính offset 3 trục Accel (trừ trọng lực `1g` ở trục Z) và 3 trục Gyro, bù offset vào dữ liệu đo. | **Báo cáo Chương 2:** Thiết kế phần cứng, sơ đồ nối dây, lưu đồ thuật toán và thiết kế chi tiết các module phần mềm. | Nguyên lý hiệu chuẩn, cách tính và bù offset, xử lý lệnh `CALIBRATE_IMU` và so sánh kết quả trước/sau hiệu chuẩn. |
| **TV3** | **Nguyễn Duy Anh** | `angle_filter.c`<br>`angle_filter.h` | Tính góc tĩnh Roll/Pitch từ gia tốc (`atan2f`), tích phân vận tốc góc Gyro theo `dt_s`, hợp nhất bằng bộ lọc Complementary Filter, quản lý hệ số `alpha`. | **Báo cáo Chương 1:** Cơ sở lý thuyết MEMS IMU, giao thức ngoại vi, bài toán trôi Gyro/nhiễu Accel và toán học bộ lọc bù. | Công thức Roll/Pitch từ Accel, tích phân Gyro, nguyên lý Complementary Filter và ảnh hưởng của hệ số `alpha`. |
| **TV4** | **Trần Tuấn Anh** | `uart_cmd.c`<br>`uart_cmd.h`<br>`flash_config.c`<br>`flash_config.h` | Nhận chuỗi lệnh UART (ngắt RX), parser xử lý 4 lệnh (`INFO_REQUEST`, `CALIBRATE_IMU`, `SET_FILTER_ALPHA`, `GET_ANGLE`), đọc/ghi cấu hình (`offset`, `alpha`, checksum) trên Flash W25Qxx. | Thiết kế Slide thuyết trình cho toàn nhóm. | Cơ chế nhận & parser lệnh UART, phản hồi thông tin nhóm + 3 lệnh điều khiển, giao tiếp SPI và cấu trúc lưu Flash W25Qxx. |
| **TV5** | **Trần Khắc Anh** | `display.c`<br>`display.h`<br>`system.c`<br>`system.h`<br>`main.c` | Cấu hình TIMER tạo chu kỳ lấy mẫu cố định (`dt_s`), viết driver hiển thị OLED/TFT, điều phối luồng chạy trong `system.c` và tích hợp toàn bộ module vào `main.c`. | **Báo cáo Chương 3:** Tích hợp hệ thống, kết quả thực nghiệm, kiểm thử lệnh UART/Flash và đánh giá sai số/độ ổn định. | Cấu hình TIMER chu kỳ cố định, luồng tích hợp hệ thống, giao diện hiển thị màn hình và demo sản phẩm hoàn chỉnh. |
| **Chung** | **Cả 5 thành viên** | `imu_types.h` | Chốt một lần các kiểu dữ liệu (`struct`), mã lỗi và hằng số dùng chung trước khi tách nhánh code. | Quay video giải thích code + demo phần cứng thật. | Trả lời vấn đáp chéo và chỉnh sửa code trực tiếp trên laptop cá nhân. |

## 4. Quy ước vật lý, hệ trục và thông số mặc định

Để các module của TV1, TV2 và TV3 khớp nhau về mặt toán học ngay từ lần ghép đầu tiên, nhóm thống nhất các quy ước sau (Tuân thủ chuẩn hàng không vũ trụ DIN 9300 / ISO 1151 Flight Dynamics):

1. **Tần số lấy mẫu và chu kỳ tính toán (`TIMER`):**
   * Tần số lấy mẫu mặc định: `IMU_SAMPLE_RATE_HZ = 100` (tương ứng chu kỳ ngắt Timer cố định `IMU_SAMPLE_PERIOD_S = 0.01f` hay `10 ms`).
2. **Dải đo và hệ số chuyển đổi MPU6050 (InvenSense RM-MPU-6000A Rev 4.2):**
   * Accelerometer: cấu hình dải `±2g` (`AFS_SEL = 0` tại Reg `0x1C` bits `[4:3]`), hệ số chia `16384.0f LSB/g`.
   * Gyroscope: cấu hình dải `±250 dps` (`FS_SEL = 0` tại Reg `0x1B` bits `[4:3]`), hệ số chia `131.0f LSB/(deg/s)`.
   * Bộ lọc thông thấp nội bộ (DLPF): cấu hình thanh ghi `CONFIG (0x1A)` bits `[2:0] DLPF_CFG = 0x03` (~44 Hz Accel BW, ~42 Hz Gyro BW) để giảm nhiễu rung cơ học trước khi đọc.
   * Quản lý nguồn (`PWR_MGMT_1` `0x6B`): xóa bit 6 `SLEEP` để đánh thức bộ dao động nội 8 MHz / PLL con quay.
3. **Quy ước đặt cảm biến và bù trọng lực khi hiệu chuẩn (TV2 lưu ý):**
   * Khi thực hiện `CALIBRATE_IMU`, thiết bị được đặt **nằm phẳng và đứng yên** trên mặt bàn ngang, mặt chip MPU6050 hướng lên trên.
   * Khi đó giá trị chuẩn mong muốn là: `ax_g = 0.0f`, `ay_g = 0.0f`, **`az_g = +1.0f`** và `gx_dps = gy_dps = gz_dps = 0.0f`.
   * Do đó, công thức tính offset gia tốc trục Z của TV2 bắt buộc là: `accel_offset_z = avg_az_g - 1.0f;` (hai trục X, Y là `avg_ax_g - 0.0f` và `avg_ay_g - 0.0f`).
4. **Quy ước công thức góc Roll và Pitch (Chuẩn DIN 9300 / ISO 1151 - TV3 lưu ý):**
   * **Roll (`roll_deg` - quay quanh trục X, rate $p$ từ Gyro X `0x43/0x44`):**
     $$\text{acc\_roll\_deg} = \text{atan2f}(a_y, a_z) \times \frac{180.0}{\pi}$$
      kết hợp với vận tốc góc trục X (`gx_dps`).
   * **Pitch (`pitch_deg` - quay quanh trục Y, rate $q$ từ Gyro Y `0x45/0x46`):**
     $$\text{acc\_pitch\_deg} = \text{atan2f}\left(-a_x, \sqrt{a_y^2 + a_z^2}\right) \times \frac{180.0}{\pi}$$
     kết hợp với vận tốc góc trục Y (`gy_dps`).
   * **Công thức Complementary Filter:**
     $$\text{roll\_deg} = \alpha \times (\text{roll\_deg} + g_x \times dt) + (1.0 - \alpha) \times \text{acc\_roll\_deg}$$
     $$\text{pitch\_deg} = \alpha \times (\text{pitch\_deg} + g_y \times dt) + (1.0 - \alpha) \times \text{acc\_pitch\_deg}$$
   * Hệ số lọc mặc định: `DEFAULT_FILTER_ALPHA = 0.98f` (hợp lệ trong khoảng `[0.50f, 0.999f]`).
5. **Đặc tả bộ nhớ NOR Flash W25Q64FV (Winbond W25Q64FV Rev J - TV4 lưu ý):**
   * Dung lượng: 8MB, 128 Block 64KB (`0xD8`), 2048 Sector 4KB (`0x20`), 32768 Page 256B (`0x02`).
   * Bảo vệ cuốn trang (Page Boundary Wrap): Ghi không vượt quá ranh giới 256 bytes của từng page (`(addr & 0xFF) + len <= 256`).
   * Thanh ghi trạng thái 1 (`0x05`): Bit 0 `BUSY` (1 = đang ghi/xóa), Bit 1 `WEL` (1 = write enabled).
   * Timeout bảo vệ: Mọi vòng lặp chờ `BUSY` phải giới hạn chu kỳ (vd: 500,000 tick) tránh treo hệ thống khi mất kết nối SPI.

## 5. Kiểu dữ liệu chung: `imu_types.h`

File `Core/Inc/imu_types.h` được chốt cố định như sau:

```c
#ifndef IMU_TYPES_H
#define IMU_TYPES_H

#include <stdint.h>
#include <stdbool.h>

/* Hằng số hệ thống mặc định */
#define IMU_SAMPLE_RATE_HZ       100U
#define IMU_SAMPLE_PERIOD_S      0.01f
#define IMU_DEFAULT_ALPHA        0.98f
#define IMU_ALPHA_MIN            0.50f
#define IMU_ALPHA_MAX            0.999f
#define IMU_CALIB_SAMPLES        200U
#define IMU_CONFIG_MAGIC         0x494D5534UL /* "IMU4" đánh dấu dữ liệu Flash hợp lệ */

/* Mã trạng thái trả về chung cho toàn bộ các module */
typedef enum {
    IMU_OK = 0,
    IMU_ERR_I2C = -1,
    IMU_ERR_DEVICE_ID = -2,
    IMU_ERR_INVALID_PARAM = -3,
    IMU_ERR_FLASH = -4,
    IMU_ERR_NOT_CALIBRATED = -5
} IMU_Status_t;

/* Dữ liệu cảm biến đã đổi sang đơn vị vật lý */
typedef struct {
    float ax_g;
    float ay_g;
    float az_g;
    float gx_dps;
    float gy_dps;
    float gz_dps;
} IMU_Data_t;

/* Góc nghiêng đầu ra sau bộ lọc (đơn vị: độ) */
typedef struct {
    float roll_deg;
    float pitch_deg;
} Angle_Data_t;

/* Giá trị sai số tĩnh (offset) của cảm biến */
typedef struct {
    float accel_offset_x;
    float accel_offset_y;
    float accel_offset_z;
    float gyro_offset_x;
    float gyro_offset_y;
    float gyro_offset_z;
} IMU_Offset_t;

/* Cấu trúc lưu trữ xuống Flash W25Qxx (có kèm magic & checksum) */
typedef struct {
    uint32_t magic;        /* Phải bằng IMU_CONFIG_MAGIC (0x494D5534) nếu Flash đã lưu */
    IMU_Offset_t offset;   /* 24 bytes offset Accel + Gyro */
    float alpha;           /* Hệ số lọc Complementary Filter */
    uint32_t crc32;        /* Checksum kiểm tra toàn vẹn dữ liệu */
} System_Config_t;

#endif /* IMU_TYPES_H */
```

## 6. Danh sách API công khai chính thức (Đã chốt kiểu trả về)

Tất cả các hàm có khả năng thất bại do phần cứng (I2C, SPI Flash) hoặc tham số đầu vào đều trả về `int` (sử dụng các giá trị trong `IMU_Status_t`: `0` = `IMU_OK`, `< 0` = lỗi) hoặc `bool` để module gọi dễ dàng kiểm tra.

| TV | Nguyên mẫu hàm (Prototype) | Kiểu trả về | Mô tả chức năng |
| :--- | :--- | :--- | :--- |
| **TV1** | `int MPU6050_Init(void);` | `int` (`0` = OK) | Kiểm tra kết nối và khởi tạo các thanh ghi MPU6050 |
| **TV1** | `int MPU6050_Read(IMU_Data_t *data);` | `int` (`0` = OK) | Đọc 6 trục Accel/Gyro và quy đổi sang đơn vị `g`, `dps` |
| **TV1** | `bool MPU6050_CheckConnection(void);` | `bool` | Đọc thanh ghi `WHO_AM_I` (`0x75`), trả về `true` nếu bằng `0x68` |
| **TV2** | `int Calibration_Run(void);` | `int` (`0` = OK) | Đọc `IMU_CALIB_SAMPLES` mẫu tĩnh, tính offset và bật cờ `is_calibrated` |
| **TV2** | `void Calibration_Apply(IMU_Data_t *data);` | `void` | Trừ trực tiếp offset vào dữ liệu `data` (nếu đã hiệu chuẩn) |
| **TV2** | `void Calibration_GetOffset(IMU_Offset_t *offset);` | `void` | Xuất cấu trúc offset hiện tại trong RAM ra ngoài (để lưu Flash) |
| **TV2** | `void Calibration_SetOffset(const IMU_Offset_t *offset);` | `void` | Nạp offset (đọc từ Flash lúc khởi động) vào RAM và bật `is_calibrated` |
| **TV2** | `bool Calibration_IsCalibrated(void);` | `bool` | Trả về trạng thái đã có dữ liệu hiệu chuẩn hợp lệ hay chưa |
| **TV3** | `void Angle_Init(float alpha);` | `void` | Khởi tạo bộ lọc góc với hệ số `alpha` ban đầu |
| **TV3** | `void Angle_Update(const IMU_Data_t *data, float dt_s);` | `void` | Tính góc Accel, tích phân Gyro và cập nhật Complementary Filter |
| **TV3** | `void Angle_Get(Angle_Data_t *angle);` | `void` | Lấy giá trị `roll_deg` và `pitch_deg` mới nhất |
| **TV3** | `int Angle_SetAlpha(float alpha);` | `int` (`0` = OK) | Kiểm tra `alpha` trong `[0.50, 0.999]` và cập nhật hệ số lọc |
| **TV3** | `float Angle_GetAlpha(void);` | `float` | Lấy giá trị `alpha` hiện tại |
| **TV4** | `void UART_Init(void);` | `void` | Khởi tạo bộ đệm nhận UART (ngắt RX / IDLE) |
| **TV4** | `void UART_Process(void);` | `void` | Kiểm tra cờ lệnh mới, phân tích cú pháp và thực thi 4 lệnh UART |
| **TV4** | `void UART_SendAngle(const Angle_Data_t *angle);` | `void` | Gửi phản hồi `ROLL:<val>\r\nPITCH:<val>\r\n` qua UART |
| **TV4** | `void UART_SendGroupInfo(void);` | `void` | Gửi 6 trường thông tin nhóm khi nhận lệnh `INFO_REQUEST` |
| **TV4** | `int Flash_Init(void);` | `int` (`0` = OK) | Khởi tạo giao tiếp SPI và kiểm tra chip nhớ W25Qxx |
| **TV4** | `int Flash_SaveConfig(const System_Config_t *config);` | `int` (`0` = OK) | Xóa sector, tính checksum và ghi cấu hình xuống W25Qxx |
| **TV4** | `int Flash_LoadConfig(System_Config_t *config);` | `int` (`0` = OK) | Đọc cấu hình từ W25Qxx, kiểm tra `magic` và `crc32` |
| **TV5** | `int Display_Init(void);` | `int` (`0` = OK) | Khởi tạo màn hình OLED/TFT và vẽ khung giao diện tĩnh |
| **TV5** | `void Display_Update(const Angle_Data_t *angle);` | `void` | Cập nhật giá trị Roll, Pitch, Alpha lên màn hình |
| **TV5** | `void System_Init(void);` | `void` | Gọi khởi tạo toàn bộ module, nạp cấu hình từ Flash và bật Timer |
| **TV5** | `void System_Update(void);` | `void` | Hàm điều phối chính đặt trong `while(1)` của `main.c` |
| **TV5** | `void System_TimerCallback(void);` | `void` | Đặt trong `HAL_TIM_PeriodElapsedCallback` để bật cờ `sample_flag = 1` |

## 7. Tên biến nội bộ khuyến nghị theo từng thành viên

| Thành viên | Tên biến chuẩn | Kiểu dữ liệu | Ý nghĩa |
| :--- | :--- | :--- | :--- |
| **TV1** | `ax_g`, `ay_g`, `az_g` | `float` | Gia tốc 3 trục theo đơn vị `g` |
| **TV1** | `gx_dps`, `gy_dps`, `gz_dps` | `float` | Vận tốc góc 3 trục theo đơn vị `deg/s` |
| **TV2** | `current_offset` | `static IMU_Offset_t` | Biến tĩnh lưu offset 6 trục trong `calibration.c` |
| **TV2** | `is_calibrated` | `static bool` | Cờ báo hiệu đã hiệu chuẩn hoặc đã nạp offset từ Flash |
| **TV3** | `roll_deg`, `pitch_deg` | `static float` | Góc nghiêng hiện tại sau bộ lọc bù |
| **TV3** | `filter_alpha` | `static float` | Hệ số lọc Complementary Filter hiện tại |
| **TV3** | `acc_roll_deg`, `acc_pitch_deg` | `float` | Góc tính tức thời từ Accelerometer |
| **TV4** | `rx_buffer`, `tx_buffer` | `char[]` | Bộ đệm chuỗi nhận/truyền UART |
| **TV4** | `sys_config` | `System_Config_t` | Biến đóng gói cấu hình khi đọc/ghi Flash |
| **TV5** | `sample_flag` | `volatile uint8_t` | Cờ báo có chu kỳ lấy mẫu mới từ ngắt TIMER |
| **TV5** | `sample_period_s` | `float` | Chu kỳ lấy mẫu cố định (`0.01f`) |
| **TV5** | `imu_data`, `angle_data` | `IMU_Data_t`, `Angle_Data_t` | Biến trung gian truyền dữ liệu giữa các module |

## 8. Quan hệ phụ thuộc (`#include`) và luồng dữ liệu

| File nguồn (`.c`) | Các file header (`.h`) được phép `#include` |
| :--- | :--- |
| `mpu6050.c` | `mpu6050.h`, `imu_types.h`, header HAL I2C (`main.h`) |
| `calibration.c` | `calibration.h`, `imu_types.h`, `mpu6050.h` (để gọi `MPU6050_Read`), header HAL (`HAL_Delay`) |
| `angle_filter.c` | `angle_filter.h`, `imu_types.h`, `<math.h>` |
| `flash_config.c` | `flash_config.h`, `imu_types.h`, header HAL SPI (`main.h`) |
| `uart_cmd.c` | `uart_cmd.h`, `imu_types.h`, `calibration.h`, `angle_filter.h`, `flash_config.h`, `<stdio.h>`, `<string.h>`, `<stdlib.h>` |
| `display.c` | `display.h`, `imu_types.h`, header HAL I2C/SPI (`main.h`) |
| `system.c` | `system.h`, `imu_types.h`, `mpu6050.h`, `calibration.h`, `angle_filter.h`, `uart_cmd.h`, `flash_config.h`, `display.h` |

### Luồng phối hợp chuẩn trong `system.c`:

```c
void System_Init(void)
{
    System_Config_t cfg;

    MPU6050_Init();
    Display_Init();
    Flash_Init();
    UART_Init();

    if (Flash_LoadConfig(&cfg) == IMU_OK) {
        Calibration_SetOffset(&cfg.offset);
        Angle_Init(cfg.alpha);
    } else {
        Angle_Init(IMU_DEFAULT_ALPHA);
    }
}

void System_Update(void)
{
    IMU_Data_t imu_data;
    Angle_Data_t angle_data;

    /* 1. Luôn kiểm tra và xử lý lệnh UART bất đồng bộ */
    UART_Process();

    /* 2. Thực hiện đúng chu kỳ cố định khi TIMER bật cờ sample_flag */
    if (sample_flag != 0U) {
        sample_flag = 0U;

        if (MPU6050_Read(&imu_data) == IMU_OK) {
            Calibration_Apply(&imu_data);
            Angle_Update(&imu_data, IMU_SAMPLE_PERIOD_S);
            Angle_Get(&angle_data);
            Display_Update(&angle_data);
        }
    }
}
```

## 9. Quy chuẩn giao thức UART (Đầu vào & Phản hồi)

Mọi lệnh nhận qua UART kết thúc bằng ký tự xuống dòng (`\n` hoặc `\r\n`). Hệ thống phải cắt bỏ ký tự `\r`, `\n` ở cuối trước khi so sánh chuỗi:

1. **Lệnh yêu cầu thông tin nhóm (Yêu cầu chung bắt buộc ở Trang 2 đề bài):**
   * Nhận: `INFO_REQUEST` hoặc `INFO_REQUEST:GROUP`
   * Phản hồi:
     ```text
     GROUP_NAME: Group 04 - MPU6050 Inclinometer
     MEMBERS: Tran Duc Anh, Tran Quoc An, Nguyen Duy Anh, Tran Tuan Anh, Tran Khac Anh
     PROJECT_NAME: Inclinometer Device Using MPU6050
     HARDWARE_USED: STM32, MPU6050, OLED/TFT, W25Qxx
     SOFTWARE_USED: STM32CubeIDE, HAL
     DATE: 2026-10-26
     ```
2. **Lệnh hiệu chuẩn cảm biến:**
   * Nhận: `CALIBRATE_IMU`
   * Hành động: Gọi `Calibration_Run()`, lấy offset mới qua `Calibration_GetOffset()`, lấy alpha qua `Angle_GetAlpha()`, đóng gói vào `System_Config_t` và gọi `Flash_SaveConfig()`.
   * Phản hồi khi thành công:
     ```text
     CALIBRATE:OK
     ACC_OFFSET:0.02,-0.01,0.05
     GYRO_OFFSET:-1.12,0.45,-0.23
     ```
3. **Lệnh cài đặt hệ số lọc:**
   * Nhận: `SET_FILTER_ALPHA:0.98`
   * Hành động: Tách chuỗi sau dấu `:`, chuyển sang `float`, gọi `Angle_SetAlpha(val)`. Nếu trả về `IMU_OK`, cập nhật `System_Config_t` và gọi `Flash_SaveConfig()`.
   * Phản hồi: `FILTER_ALPHA:0.98` (hoặc `ERROR:INVALID_ALPHA` nếu ngoài ngưỡng `[0.50, 0.999]`).
4. **Lệnh đọc góc nghiêng:**
   * Nhận: `GET_ANGLE`
   * Hành động: Gọi `Angle_Get(&angle)` và `UART_SendAngle(&angle)`.
   * Phản hồi (đúng định dạng đề bài trang 6):
     ```text
     ROLL:12.4
     PITCH:-5.8
     ```

## 10. Quy trình làm việc với GitHub và tích hợp

1. **Khởi tạo nền tảng:** TV5 tạo project STM32CubeIDE, cấu hình file `.ioc`, tạo sẵn `imu_types.h` và các file `.h`/`.c` khung rồi đẩy lên nhánh `main`.
2. **Phát triển theo nhánh:** Mỗi thành viên tạo branch riêng (`tv1-mpu6050`, `tv2-calibration`, `tv3-filter`, `tv4-uart-flash`, `tv5-system-display`) và chỉ sửa các file thuộc quyền sở hữu của mình.
3. **Bảo vệ file chung:** Không ai tự ý sửa `imu_types.h`, `.ioc` hoặc `main.c` trên nhánh cá nhân mà không thông báo cho cả nhóm.
4. **Ghép code từng bước:** Mở Pull Request vào nhánh `main`, kiểm tra biên dịch không lỗi/cảnh báo rồi nạp test ngay trên mạch thật sau mỗi lần merge.

## 11. Chỉ dẫn bắt buộc dành cho AI của từng thành viên

Khi nhận tài liệu này, AI hỗ trợ lập trình cho từng thành viên phải:

1. Xác định rõ người dùng là thành viên nào (**TV1 – TV5**) và chỉ viết/sửa đúng các file thuộc trách nhiệm của thành viên đó (trừ khi được yêu cầu hỗ trợ tích hợp).
2. Tuân thủ tuyệt đối tên file, kiểu dữ liệu trong `imu_types.h`, nguyên mẫu hàm trong Mục 6 và tên biến trong Mục 7.
3. Khi cần gọi sang module của thành viên khác, chỉ `#include` file `.h` tương ứng và gọi đúng nguyên mẫu hàm đã chốt.
4. Luôn kèm theo hướng dẫn kiểm thử độc lập (Unit Test / Mock Test) cho module vừa viết để thành viên đó có thể test ngay cả khi chưa ghép đủ phần cứng của người khác.

### Mẫu prompt để mỗi thành viên copy gửi cho AI

> Tôi là **TV[1–5] ([Họ và tên])** của nhóm làm Bài 4 Vi xử lý (Thiết bị đo góc nghiêng dùng MPU6050). Hãy đọc kỹ file `QUY_TAC_CODE_NHOM_MPU6050.md` đính kèm như quy chuẩn lập trình bắt buộc của nhóm. Tôi phụ trách file **`[tên_file.c/h]`**. Hãy viết đầy đủ file `.h` và `.c` bằng ngôn ngữ C (STM32 HAL) tuân thủ 100% kiểu dữ liệu trong `imu_types.h` và nguyên mẫu hàm ở Mục 6. Đồng thời giải thích từng đoạn code để tôi đi bảo vệ và hướng dẫn cách test độc lập module của tôi.
