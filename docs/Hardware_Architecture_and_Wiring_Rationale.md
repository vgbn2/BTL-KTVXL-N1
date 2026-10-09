# Hardware Architecture, Pin Allocation & Wiring Rationale

Bài tập lớn Kỹ thuật Vi xử lý - Nhóm 1 - D24-PTIT  
**Đề tài:** Thiết bị đo góc nghiêng sử dụng MPU6050, Flash W25Qxx, STM32F103C8T6, và ESP32 + Màn hình ILI9341 320x240 SPI TFT.

---

## 1. Tổng Quan Kiến Trúc Hệ Thống (Architectural Rationale)

Hệ thống được thiết kế theo mô hình **Phân tách trách nhiệm kép (Dual-Processor Workload Partitioning)**:

```
+---------------------------------------------------------------------------------------------------------+
|                                           TOÀN BỘ HỆ THỐNG                                              |
+---------------------------------------------------+-----------------------------------------------------+
|         STM32F103C8T6 (Sensor & Math Engine)       |            ESP32-WROOM-32 (Display & UI Host)       |
+---------------------------------------------------+-----------------------------------------------------+
| 1. Lấy mẫu cảm biến MPU6050 chính xác (100 Hz ISR)| 1. Quản lý hiển thị màn hình màu ILI9341 (320x240)  |
| 2. Tính toán bộ lọc bù (Complementary Filter)     | 2. Vẽ đồ thị thời gian thực, bảng điều khiển UI     |
| 3. Đọc/Ghi thông số hiệu chuẩn vào Flash W25Qxx   | 3. Nhận lệnh từ người dùng & gửi cấu hình qua UART  |
| 4. Xuất telemetry góc Roll/Pitch qua UART         | 4. Mở rộng kết nối không dây (WiFi / BLE)           |
+---------------------------------------------------+-----------------------------------------------------+
```

### Tại sao không dùng 1 vi điều khiển duy nhất?
1. **Yêu cầu tính toán thời gian thực không trễ (Deterministic Hard Real-Time):**
   * Thuật toán ước lượng góc nghiêng sử dụng tích phân vận tốc góc:
     $$\theta(t) = \alpha \cdot (\theta(t - \Delta t) + \omega \cdot \Delta t) + (1 - \alpha) \cdot \theta_{\text{accel}}$$
   * Chu kỳ lấy mẫu $\Delta t$ bắt buộc phải là hằng số chính xác ($10\text{ ms} \pm 0\text{ }\mu\text{s}$). Nếu vi điều khiển vừa phải quét màn hình SPI (mất 20–50 ms cho mỗi khung hình), chu kỳ $\Delta t$ sẽ bị giật (jitter), dẫn đến sai số tích phân phân kỳ rất nhanh.
2. **Tải bộ nhớ & Băng thông màn hình:**
   * Màn hình TFT $320 \times 240$ màu 16-bit (RGB565) cần $320 \times 240 \times 2 = 153.6\text{ KB}$ cho 1 frame buffer đầy đủ.
   * STM32F103C8T6 chỉ có **20 KB SRAM**, hoàn toàn không đủ chứa frame buffer.
   * ESP32 có **520 KB SRAM** và 2 nhân 240 MHz, xử lý đồ họa mượt mà mà không ảnh hưởng đến vòng lặp đo góc.

---

## 2. Giải Thích Chi Tiết Từng Khối Đấu Nối & Chân Giao Tiếp

### 2.1 Khối Cảm Biến: STM32 ⟷ MPU6050 (GY-521 qua I2C1)

```
        STM32F103C8T6                               GY-521 (MPU6050)
     +-----------------+                         +--------------------+
     |   PB6 (I2C1_SCL)├────────────────────────►│ SCL  (Pin 3)       |
     |   PB7 (I2C1_SDA)├◄───────────────────────►│ SDA  (Pin 4)       |
     |   3.3V Power Out├────────────────────────►│ VCC  (Pin 1)       |
     |   GND           ├────────────────────────►│ GND  (Pin 2)       |
     |                 |                         │ AD0  (Pin 7) ──+   |
     |                 |                         |                |   |
     |                 |                         +----------------+---+
     +-----------------+                                          |
                                                                 GND (Address = 0x68)
```

| Chân GY-521 | Chân STM32 | Lý Do Thiết Kế & Nguyên Lý Hoạt Động |
| :--- | :--- | :--- |
| **Pin 3: SCL** | **PB6** | Chân phần cứng gốc của ngoại vi **`I2C1_SCL`** trên STM32. STM32 tự động tạo xung nhịp chuẩn (100kHz Standard / 400kHz Fast Mode) bằng phần cứng, không tốn tài nguyên CPU bit-banging. |
| **Pin 4: SDA** | **PB7** | Chân phần cứng gốc **`I2C1_SDA`**. Hỗ trợ cơ chế Open-Drain chuẩn I2C, phần cứng tự kiểm tra cờ ACK/NACK và chống xung đột đường truyền. |
| **Pin 1: VCC** | **3.3V** | Cấp nguồn 3.3V từ mạch hạ áp của STM32. GY-521 có IC ổn áp KB33/AMS1117 tích hợp sẵn, cấp 3.3V trực tiếp giúp cảm biến chạy mát nhất và nguồn ổn định, ít nhiễu ADC. |
| **Pin 2: GND** | **GND** | Đường mass hồi tiếp cho tín hiệu I2C. |
| **Pin 7: AD0** | **GND** | Chọn địa chỉ I2C của chip: **Nối GND = `0x68`**, **Nối 3.3V = `0x69`**. **Bắt buộc nối cố định vào GND**, nếu để chân này thả nổi (floating), nhiễu tĩnh điện môi trường sẽ làm địa chỉ nhảy loạn giữa 0x68 và 0x69, gây treo bus I2C. |
| **Pin 5 & 6 (XDA/XCL)**| **NC** | Đây là đường I2C Master phụ để nối thêm la bàn từ trường ngoài (HMC5883L). Dự án đo góc 6-axis không dùng nên **để trống**. |
| **Pin 8: INT** | **PB5** (Tùy chọn)| Chân ngắt Data-Ready của MPU6050. Báo ngắt phần cứng mỗi khi có mẫu đo mới hoàn tất trong thanh ghi. |

---

### 2.2 Khối Bộ Nhớ Flash: STM32 ⟷ W25Qxx (W25Q64 qua SPI1)

```
        STM32F103C8T6                              W25QXX (W25Q64)
     +-----------------+                         +--------------------+
     |   PA4 (SPI1_NSS)├────────────────────────►│ /CS   (Pin 1)      |
     |   PA5 (SPI1_SCK)├────────────────────────►│ CLK   (Pin 6)      |
     |   PA6 (SPI1_MISO├◄────────────────────────┤ DO    (Pin 2)      |
     |   PA7 (SPI1_MOSI├────────────────────────►│ DI    (Pin 5)      |
     |   3.3V Power Out├───────────+────────────►│ VCC   (Pin 8)      |
     |                 |           ├────────────►│ /WP   (Pin 3)      |
     |                 |           └────────────►│ /HOLD (Pin 7)      |
     |   GND           ├────────────────────────►│ GND   (Pin 4)      |
     +-----------------+                         +--------------------+
```

| Chân W25Qxx | Chân STM32 | Lý Do Kỹ Thuật |
| :--- | :--- | :--- |
| **Pin 1: /CS** | **PA4** | Chân Chip Select ngoại vi **`SPI1_NSS`**. Được điều khiển bằng phần mềm (Software NSS): Kéo xuống `LOW` trước khi gửi lệnh SPI, kéo lên `HIGH` khi kết thúc gói tin để chip thực thi lệnh. |
| **Pin 6: CLK** | **PA5** | Xung nhịp **`SPI1_SCK`** từ STM32 (Bus APB2 lên đến 36 MHz). Tốc độ đọc/ghi Flash cực nhanh (vài micro giây cho 1 block offset). |
| **Pin 2: DO** | **PA6** | Đường dữ liệu **`SPI1_MISO`** (Master In Slave Out): Dữ liệu đọc từ Flash truyền về STM32 (đọc offset khi khởi động). |
| **Pin 5: DI** | **PA7** | Đường dữ liệu **`SPI1_MOSI`** (Master Out Slave In): STM32 gửi lệnh đọc/ghi/xóa sector và truyền hệ số lọc $\alpha$, offset sang Flash. |
| **Pin 3: /WP** | **3.3V** | **Write Protect (Active LOW)**: Phải nối lên mức cao (3.3V) để cho phép ghi dữ liệu hiệu chuẩn vào Flash. Nếu nối GND hoặc thả nổi, chip sẽ bị khóa chống ghi. |
| **Pin 7: /HOLD**| **3.3V** | **Hold / Pause (Active LOW)**: Phải nối lên 3.3V để chip hoạt động bình thường. Nếu thả nổi hoặc kéo xuống 0V, giao tiếp SPI của chip sẽ bị tạm ngưng. |
| **Pin 8: VCC** | **3.3V** | **CHỈ ĐƯỢC CẤP 3.3V (2.7V - 3.6V)**. Tuyệt đối không cắm vào nguồn 5V vì sẽ phá hủy bán dẫn của chip Flash. |

---

### 2.3 Khối Cầu Nối Liên Vi Xử Lý: STM32 ⟷ ESP32 (UART Link)

```
      STM32F103 (USART1)                         ESP32 (UART2)
     +------------------+                     +------------------+
     |   PA9  (TX1) ────┼────────────────────►│ GPIO 16 (RX2)    |  --> Dữ liệu: ROLL:xx.x, PITCH:yy.y
     |   PA10 (RX1) ◄───┼─────────────────────┤ GPIO 17 (TX2)    |  <-- Lệnh: CALIBRATE_IMU, SET_FILTER_ALPHA
     |   GND        ────┼─────────────────────┤ GND              |  <-- Mass chung (Reference)
     +------------------+                     +------------------+
```

| Tín Hiệu | STM32 | ESP32 | Lý Do Kỹ Thuật & Cấu Hình |
| :--- | :--- | :--- | :--- |
| **TX $\rightarrow$ RX** | **PA9** | **GPIO 16** | STM32 phát dòng dữ liệu góc nghiêng liên tục (`115200 bps`, `8N1`). ESP32 nhận bằng ngắt UART hoặc FreeRTOS Task. |
| **RX $\leftarrow$ TX** | **PA10** | **GPIO 17** | ESP32 gửi các lệnh cấu hình xuống STM32 (kích hoạt căn chỉnh cảm biến hoặc chỉnh hệ số $\alpha$). |
| **Mass chung** | **GND** | **GND** | **Bắt buộc**: Điện áp 3.3V là hiệu điện thế so với Mass. Không có dây GND chung, mức logic 0 và 1 sẽ bị trôi, gây lỗi UART framing error hoặc mất dữ liệu. |
| **Tại sao dùng UART2 trên ESP32?** | - | `GPIO 16/17` | `UART0` (`GPIO 1/3`) của ESP32 được nối với chip nạp USB để nạp code và in Serial Monitor máy tính. Dùng `UART2` giúp kênh giao tiếp với STM32 hoàn toàn độc lập, không bị xung đột với cổng USB máy tính. |

---

### 2.4 Khối Hiển Thị: ESP32 ⟷ Màn Hình ILI9341 2.8" SPI TFT

```
          ESP32-WROOM-32                       ILI9341 2.8" SPI TFT (240x320)
     +----------------------+                 +-------------------------------+
     | GPIO 15 (VSPI_CS)    ├────────────────►│ CS     (Pin 3) - Chọn màn hình|
     | GPIO 2  (TFT_DC)     ├────────────────►│ DC/RS  (Pin 5) - Lệnh/Dữ liệu |
     | GPIO 4  (TFT_RST)    ├────────────────►│ RESET  (Pin 4) - Reset IC LCD |
     | GPIO 18 (VSPI_SCK)   ├────────────────►│ SCK    (Pin 7) - Xung nhịp SPI|
     | GPIO 23 (VSPI_MOSI)  ├────────────────►│ SDI    (Pin 6) - Dữ liệu điểm |
     | 5V / VIN Rail        ├────────────────►│ VCC    (Pin 1) - Nguồn chính  |
     | 3.3V Rail (hoặc PWM) ├────────────────►│ LED    (Pin 8) - Đèn nền LCD  |
     | GND Rail             ├────────────────►│ GND    (Pin 2) - Nối Mass     |
     +----------------------+                 +-------------------------------+
```

| Chân TFT | Chân ESP32 | Lý Do Kỹ Thuật |
| :--- | :--- | :--- |
| **CS (Pin 3)** | **GPIO 15** | Chip Select của màn hình. Mức `LOW` kích hoạt chip driver đồ họa ILI9341 lắng nghe dữ liệu từ bus SPI. |
| **DC/RS (Pin 5)** | **GPIO 2** | **Data / Command Select**: Khi mức `LOW`, byte gửi qua SPI là lệnh khởi tạo/cài đặt cửa sổ vẽ (`0x2A`, `0x2B`, `0x2C`). Khi mức `HIGH`, byte gửi qua SPI là mã màu điểm ảnh (Pixel Color RGB565). |
| **RESET (Pin 4)**| **GPIO 4** | Chân Reset phần cứng. ESP32 kéo chân này xuống `LOW` 50ms khi bật nguồn để reset trạng thái bên trong của IC ILI9341 về mặc định. |
| **SCK (Pin 7)** | **GPIO 18** | Ngoại vi **`VSPI_CLK`** của ESP32. Chạy tần số cao từ **10 MHz đến 40 MHz**, đảm bảo tốc độ refresh khung hình mượt mà. |
| **SDI (Pin 6)** | **GPIO 23** | Ngoại vi **`VSPI_MOSI`** của ESP32. Dòng dữ liệu pixel màu liên tục từ ESP32 đổ sang màn hình. |
| **LED (Pin 8)** | **3.3V / GPIO 32**| Nguồn cấp đèn nền (Backlight). Nối 3.3V để sáng 100%, hoặc nối chân PWM (GPIO 32) để chỉnh độ sáng màn hình bằng phần mềm. |
| **VCC (Pin 1)** | **5V / VIN** | Cấp nguồn chính cho board TFT. Phần lớn board ILI9341 đỏ có tích hợp sẵn IC hạ áp 3.3V (U1) trên mạch, nên nối vào chân 5V của ESP32 để màn hình hoạt động ổn định nhất. |

---

## 3. Bảng Cân Bằng Năng Lượng & Dòng Điện (Power Budget)

Toàn bộ hệ thống được cấp nguồn qua cổng USB 5V (từ củ sạc 5V/1A hoặc cổng USB máy tính 500mA):

| Linh Kiện / Khối | Điện Áp Hoạt Động | Dòng Tiêu Thụ Điển Hình | Ghi Chú |
| :--- | :--- | :--- | :--- |
| **STM32F103C8T6** | 3.3V (từ LDO onboard) | ~30 – 40 mA | Chạy 72 MHz Cortex-M3 |
| **MPU6050 (GY-521)** | 3.3V | ~3.5 – 4.0 mA | Hoạt động đầy đủ 6 trục + DMP |
| **W25Q64 SPI Flash** | 3.3V | ~4 – 15 mA | Lúc đọc ~4mA, lúc ghi/xóa sector ~15mA |
| **ESP32-WROOM-32** | 3.3V (từ LDO onboard) | ~80 – 160 mA | Xử lý giao diện & đồ họa |
| **ILI9341 LCD + Đèn nền**| 5V / 3.3V | ~60 – 90 mA | Đèn nền LED tiêu thụ chính |
| **TỔNG CỘNG HỆ THỐNG** | **5V Input** | **~180 – 310 mA** | **Rất an toàn cho cổng USB 500mA** |

---

## 4. Phân Tích Các Lỗi Đấu Nối Phổ Biến & Cách Khắc Phục (FMEA)

| Tình Huống Sai Sót | Hiện Tượng Quan Sát Được | Nguyên Nhân Kỹ Thuật | Cách Xử Lý |
| :--- | :--- | :--- | :--- |
| **Quên nối chung dây GND** | STM32 và ESP32 chạy nhưng ESP32 không nhận được dữ liệu, hoặc in ra ký tự rác `???` | Không có điện thế tham chiếu chung giữa hai chip | Nối 1 dây từ chân GND của STM32 sang chân GND của ESP32 |
| **Chân AD0 của GY-521 thả nổi** | Khởi động lúc được lúc không; hàm `MPU6050_Init()` báo lỗi `Device not found` | Địa chỉ I2C bị nhảy ngẫu nhiên giữa `0x68` và `0x69` | Hàn hoặc cắm dây từ chân **AD0 nối thẳng vào chân GND** |
| **Cấp nhầm nguồn 5V vào W25Qxx** | Chip Flash nóng ran, không đọc/ghi được, cháy chip vĩnh viễn | W25Qxx có điện áp tối đa chỉ là 3.6V | **Chỉ cắm chân VCC của W25Qxx vào chân 3.3V** của STM32 |
| **Chân /WP hoặc /HOLD của Flash thả nổi** | Đọc được ID chip (`0xEF4017`) nhưng không ghi được dữ liệu, hoặc bus SPI bị đơ | /WP ở mức thấp kích hoạt chống ghi; /HOLD ở mức thấp ngắt SPI | Nối cả 2 chân **/WP và /HOLD lên chân 3.3V** |
| **Màn hình TFT chỉ sáng trắng** | Đèn nền sáng nhưng không hiện chữ hoặc đồ họa | Sai chân CS, DC, RST hoặc tốc độ SPI quá cao | Kiểm tra đúng GPIO 15 (CS), GPIO 2 (DC), GPIO 4 (RST), GPIO 18 (SCK), GPIO 23 (MOSI) |
