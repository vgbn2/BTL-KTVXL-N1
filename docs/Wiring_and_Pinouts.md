# System Wiring, Pinouts, and Interconnection Reference

Bài tập lớn Kỹ thuật Vi xử lý - Nhóm 1 - D24-PTIT
**Đề tài:** Thiết bị đo góc nghiêng sử dụng MPU6050, Flash W25Qxx, STM32F103C8T6, và ESP32 + Màn hình ILI9341 TFT.

---

## 1. Kiến Trúc Toàn Hệ Thống (System Architecture)

```
                                  +-------------------+
                                  | 5V / USB Power    |
                                  +---------+---------+
                                            |
                         +------------------+------------------+
                         | (5V / 3.3V Rail)                    | (5V Rail)
                         v                                     v
               +-------------------+                 +-------------------+
               |  STM32F103C8T6    |                 |   ESP32-WROOM-32  |
               |  (Sensor Engine)  |                 |  (Display & IoT)  |
               +---------+---------+                 +---------+---------+
                         |                                     |
         +---------------+---------------+                     | (VSPI: GPIO 15, 2, 4, 18, 23)
         | (I2C1: PB6, PB7)              | (SPI1: PA4..PA7)    v
         v                               v           +-------------------+
+-----------------+             +-----------------+  | ILI9341 TFT 240x320|
| MPU6050 (GY-521)|             | W25Q64 Flash    |  +-------------------+
| Accel + Gyro    |             | Calib & Config  |
+-----------------+             +-----------------+
                         ^                                     ^
                         |           UART Protocol             |
                         +=====================================+
                           STM32 PA9 (TX1) <--> ESP32 GPIO 16 (RX2)
                           STM32 PA10 (RX1) <--> ESP32 GPIO 17 (TX2)
                           GND <--------------> GND (Common Ground)
```

---

## 2. Sơ Đồ Chân Chi Tiết (Pinout Diagrams)

### 2.1 STM32F103C8T6 (Blue Pill)
```
                          +-----[ USB ]-----+
             [B12]  PB12 -| [ ]         [ ] |- GND    [System GND]
      (SPI2) [B13]  PB13 -|                 |- GND    [System GND]
      (SPI2) [B14]  PB14 -|                 |- 3V3    [3.3V Output]
      (SPI2) [B15]  PB15 -|                 |- NRST   [Reset]
             [A8]   PA8  -|                 |- PB11   [B11] (I2C2 SDA)
(USART1 TX)  [A9]   PA9  -|                 |- PB10   [B10] (I2C2 SCL)
(USART1 RX)  [A10]  PA10 -|                 |- PB1    [B1]
      (USB-) [A11]  PA11 -|   STM32F103     |- PB0    [B0]
      (USB+) [A12]  PA12 -|     C8T6        |- PA7    [A7]  (SPI1 MOSI)
(SWDIO)      [A13]  PA13 -|                 |- PA6    [A6]  (SPI1 MISO)
(SWCLK)      [A14]  PA14 -|                 |- PA5    [A5]  (SPI1 SCK)
             [A15]  PA15 -|                 |- PA4    [A4]  (SPI1 CS)
             [B3]   PB3  -|                 |- PA3    [A3]  (USART2 RX)
             [B4]   PB4  -|                 |- PA2    [A2]  (USART2 TX)
             [B5]   PB5  -|                 |- PA1    [A1]
(I2C1 SCL)   [B6]   PB6  -|                 |- PA0    [A0]
(I2C1 SDA)   [B7]   PB7  -|                 |- PC15   [C15]
             [B8]   PB8  -|                 |- PC14   [C14]
             [B9]   PB9  -|                 |- PC13   [C13] (Onboard LED)
             [5V]   5V   -|                 |- VBAT
             [GND]  GND  -|                 |- GND
             [3V3]  3.3V -|                 |- 3.3V
                          +-----------------+
```

### 2.2 ESP32 DevKit (30-Pin Type-C Module)
```
                          +-----[ TYPE-C ]-----+
           (Reset)   EN  -| [ ]            [ ] |- D23  (GPIO 23 / VSPI MOSI)
           (ADC)     VP  -|                    |- D22  (GPIO 22 / I2C SCL)
           (ADC)     VN  -|                    |- TX0  (GPIO 1  / Debug TX)
           (Input)   D34 -|                    |- RX0  (GPIO 3  / Debug RX)
           (Input)   D35 -|                    |- D21  (GPIO 21 / I2C SDA)
           (ADC/DAC) D32 -|                    |- D19  (GPIO 19 / VSPI MISO)
           (ADC/DAC) D33 -|     ESP-WROOM-32   |- D18  (GPIO 18 / VSPI SCK)
           (DAC1)    D25 -|     CH340 (30P)    |- D5   (GPIO 5)
           (DAC2)    D26 -|                    |- TX2  (GPIO 17 / UART2 TX)
           (ADC)     D27 -|                    |- RX2  (GPIO 16 / UART2 RX)
           (HSPI)    D14 -|                    |- D4   (GPIO 4  / TFT RST)
           (HSPI)    D12 -|                    |- D2   (GPIO 2  / TFT DC)
           (HSPI)    D13 -|                    |- D15  (GPIO 15 / TFT CS)
           (Power)   GND -|                    |- GND  (Ground)
(5V Input) (Power)   VIN -|                    |- 3V3  (3.3V Power Out)
                          +--------------------+
```

### 2.3 GY-521 (MPU6050) 8-Pin Breakout Pinout
```
         +-----------------------+
         |     GY-521 (MPU6050)  |
   [1] --| VCC  (3.3V - 5V)      |  --> STM32 3.3V
   [2] --| GND  (Ground)         |  --> STM32 GND
   [3] --| SCL  (I2C Clock)      |  --> STM32 PB6 (I2C1_SCL)
   [4] --| SDA  (I2C Data)       |  --> STM32 PB7 (I2C1_SDA)
   [5] --| XDA  (Aux I2C Data)   |  (NC - Không nối)
   [6] --| XCL  (Aux I2C Clock)  |  (NC - Không nối)
   [7] --| AD0  (Address LSB)    |  --> Nối GND (Địa chỉ 0x68)
   [8] --| INT  (Interrupt Out)  |  --> STM32 PB5 (Tùy chọn)
         +-----------------------+
```

### 2.4 Module Flash W25Q64 (6-Pin Breakout - [Lập Trình Nhúng A-Z][135])
```
         +-----------------------+
         |   W25Q64 SPI MODULE   |
   [1] --| VCC  (Power Supply)   |  --> STM32 3.3V (2.7V - 3.6V CHỈ 3.3V!)
   [2] --| CS   (Chip Select)    |  --> STM32 PA4 (SPI1_NSS)
   [3] --| DO   (MISO Data Out)  |  --> STM32 PA6 (SPI1_MISO)
   [4] --| GND  (Ground)         |  --> STM32 GND
   [5] --| CLK  (SPI Clock)      |  --> STM32 PA5 (SPI1_SCK)
   [6] --| DI   (MOSI Data In)   |  --> STM32 PA7 (SPI1_MOSI)
         |                       |
         | * Onboard SMD pull-up |
         |   resistors on WP# &  |
         |   HOLD# to VCC        |
         +-----------------------+
```

### 2.5 Màn Hình ILI9341 2.8" SPI TFT (Chuẩn Red PCB 14-Pin / 18-Pin Header)
```
           +----------------------------------------+
           |          ILI9341 2.8" SPI TFT          |
           |  [Header Pins]     [ LCD Screen Area ] |
     [1] --|  VCC (3.3V/5V)   +-------------------+ |  --> ESP32 5V / 3.3V (Nguồn chính)
     [2] --|  GND (Ground)    |                   | |  --> ESP32 GND (Mass chung)
     [3] --|  CS              |                   | |  --> ESP32 GPIO 15 (VSPI_CS)
     [4] --|  RESET / RST     |    320 x 240      | |  --> ESP32 GPIO 4  (Reset màn hình)
     [5] --|  DC / RS         |    COLOR TFT      | |  --> ESP32 GPIO 2  (Command/Data)
     [6] --|  SDI / MOSI      |    DISPLAY        | |  --> ESP32 GPIO 23 (VSPI_MOSI)
     [7] --|  SCK / CLK       |                   | |  --> ESP32 GPIO 18 (VSPI_SCK)
     [8] --|  LED / BLK       |                   | |  --> ESP32 3.3V / GPIO 5 (Đèn nền)
     [9] --|  SDO / MISO      +-------------------+ |  --> ESP32 GPIO 19 (Tùy chọn)
           |                                        |
           |  * Chân cảm ứng XPT2046 phụ:           |
    [10] --|  T_CLK           (NC - Không nối)      |
    [11] --|  T_CS            (NC - Không nối)      |
    [12] --|  T_DIN           (NC - Không nối)      |
    [13] --|  T_DO            (NC - Không nối)      |
    [14] --|  T_IRQ           (NC - Không nối)      |
           +----------------------------------------+
```

---

## 3. Bảng Đấu Nối Từng Khối (Wiring Connection Tables)

### 3.1 STM32F103 ⟷ Cảm Biến MPU6050 (GY-521 qua I2C1)
| MPU6050 Pin | STM32 Pin | Chức Năng | Chú Thích Kỹ Thuật |
| :--- | :--- | :--- | :--- |
| **VCC** | **3.3V** | Nguồn | Cấp 3.3V trực tiếp giúp cảm biến chạy mát và ổn định |
| **GND** | **GND** | Ground chung | Nối chung mass toàn hệ thống |
| **SCL** | **PB6** | I2C1_SCL | 5V-tolerant, GY-521 đã có trở kéo 4.7kΩ lên 3.3V |
| **SDA** | **PB7** | I2C1_SDA | 5V-tolerant, GY-521 đã có trở kéo 4.7kΩ lên 3.3V |
| **AD0** | **GND** | Địa chỉ I2C | Nối GND để chọn địa chỉ `0x68` (Nối 3.3V là `0x69`) |
| **INT** | **PB5** (Tùy chọn)| Ngắt dữ liệu | Báo khi có mẫu đo mới (100Hz Data Ready) |

---

### 3.2 STM32F103 ⟷ Module Flash W25Q64 (6-Pin Breakout qua SPI1)
| W25Q64 Pin | STM32 Pin | Chức Năng | Điện Áp & Lưu Ý |
| :--- | :--- | :--- | :--- |
| **VCC** | **3.3V** | Nguồn (2.7V - 3.6V) | **Chỉ cấp 3.3V**, KHÔNG cấp 5V vào chip W25Q |
| **CS / NSS** | **PA4** | SPI1 Chip Select | Xuất mức LOW khi truyền/nhận lệnh SPI |
| **DO / MISO**| **PA6** | SPI1 Master In | Dữ liệu từ Flash đọc về STM32 (Bias calibration) |
| **GND** | **GND** | Ground | Nối chung mass toàn hệ thống |
| **CLK / SCK**| **PA5** | SPI1 Clock | Xung nhịp SPI Master (tối đa 36 MHz trên APB2) |
| **DI / MOSI**| **PA7** | SPI1 Master Out | Dữ liệu từ STM32 ghi sang Flash (Lưu offset & alpha) |
| *WP# / HOLD#*| *Tích hợp sẵn*| Auto Pull-Up | Board breakout đã hàn sẵn trở kéo lên VCC |

---

### 3.3 STM32F103 ⟷ ESP32 (Giao Tiếp UART)
| Chân STM32 | Chân ESP32 | Mức Logic | Mục Đích Truyền Nhận |
| :--- | :--- | :--- | :--- |
| **PA9 (USART1_TX)** | **GPIO 16 (UART2_RX)**| 3.3V | STM32 gửi góc Roll/Pitch (`ROLL:12.4`, `PITCH:-5.8`) |
| **PA10 (USART1_RX)**| **GPIO 17 (UART2_TX)**| 3.3V | ESP32 gửi lệnh cài đặt (`CALIBRATE_IMU`, `SET_FILTER_ALPHA:0.98`) |
| **GND** | **GND** | Reference | **Bắt buộc** nối chung Mass |

---

### 3.4 ESP32 ⟷ Màn Hình ILI9341 2.8" SPI TFT (Chuẩn `src/TftTest.ino`)
| Chân Màn Hình TFT | Chân ESP32 | Mức Điện Áp | Ngoại Vi ESP32 | Chức Năng |
| :--- | :--- | :--- | :--- | :--- |
| **VCC** | **5V** / **3.3V** | Nguồn | Nguồn chính | Nối 5V nếu board TFT có IC nguồn 3.3V (U1) |
| **GND** | **GND** | Ground | Nối mass chung | Đường hồi tiếp nguồn |
| **CS** | **GPIO 15** | 3.3V | VSPI CS | Chọn chip màn hình (Active LOW) |
| **RESET / RST** | **GPIO 4** | 3.3V | GPIO Output | Reset phần cứng khi khởi động |
| **DC / RS** | **GPIO 2** | 3.3V | GPIO Output | LOW = Lệnh (Command), HIGH = Dữ liệu (Data) |
| **SDI / MOSI** | **GPIO 23** | 3.3V | VSPI MOSI | Đường truyền dữ liệu SPI Master Out |
| **SCK / CLK** | **GPIO 18** | 3.3V | VSPI SCK | Xung nhịp SPI Clock (10 MHz - 40 MHz) |
| **LED / BL** | **3.3V** / **GPIO 32**| 3.3V | Đèn nền | Nối 3.3V hoặc băm xung PWM điều chỉnh độ sáng |
| **SDO / MISO** | **GPIO 19** (Tùy chọn)| 3.3V | VSPI MISO | Đọc dữ liệu từ màn hình (có thể để trống) |

---

### 3.5 Mạch Nạp STM32 ST-Link V2
| Chân ST-Link V2 | Chân STM32 Blue Pill |
| :--- | :--- |
| **3.3V** | **3.3V** |
| **SWDIO** | **PA13** |
| **SWCLK** | **PA14** |
| **GND** | **GND** |

---

## 4. Giao Thức Truyền Thông & Lệnh Điều Khiển (UART Commands)

| Chiều | Lệnh / Dữ Liệu | Ý Nghĩa |
| :--- | :--- | :--- |
| **ESP32 ➔ STM32** | `CALIBRATE_IMU\n` | Yêu cầu STM32 tính toán offset tĩnh của cảm biến |
| **ESP32 ➔ STM32** | `SET_FILTER_ALPHA:0.98\n` | Đặt hệ số bộ lọc bù Complementary Filter |
| **ESP32 ➔ STM32** | `GET_ANGLE\n` | Yêu cầu đọc góc hiện tại |
| **STM32 ➔ ESP32** | `ROLL:12.4\n` | Góc nghiêng Roll tính toán theo chu kỳ Timer (100Hz) |
| **STM32 ➔ ESP32** | `PITCH:-5.8\n` | Góc nghiêng Pitch tính toán theo chu kỳ Timer (100Hz) |
| **STM32 ➔ ESP32** | `CALIB_OK:OFF_AX:12,OFF_AY:-3,OFF_GZ:5\n` | Phản hồi hiệu chuẩn thành công & đã lưu Flash W25Qxx |
