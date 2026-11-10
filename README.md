# BTL-KTVXL-N1 — STM32F103 MPU6050 Inclinometer

**Bài tập lớn môn Kỹ thuật Vi xử lý – Nhóm 1 (D24-PTIT)** — Học viện Công nghệ Bưu chính Viễn thông (Khoa Kỹ thuật Điện tử 1)

Repository: [vgbn2/BTL-KTVXL-N1](https://github.com/vgbn2/BTL-KTVXL-N1)

Thiết bị đo góc nghiêng **Roll/Pitch dùng một vi điều khiển duy nhất (Single-MCU) STM32F103C8T6**, viết bằng C99 cùng STM32 Standard Peripheral Library (SPL V3.5.0) và CMSIS V1.30 đóng gói sẵn trong `third_party/stm32f10x/` ([ghi chú phát hành SPL/CMSIS](third_party/stm32f10x/Release_Notes.html)). MCU đọc gia tốc/con quay từ MPU6050 qua I2C mềm, hiệu chỉnh sai số tĩnh, chạy bộ lọc bù, hiển thị trên màn hình SPI TFT ILI9341, lưu cấu hình vào Flash W25Qxx và giao tiếp lệnh qua USART1.

```text
MPU6050 ── I2C mềm (PB6/PB7) ──► STM32F103C8T6 (72 MHz) ── SPI2 (9 MHz) ──► ILI9341 (320×240)
                                        │          │
                                  SPI1 (9 MHz)  USART1 (115200 8-N-1)
                                        │          │
                                     W25Qxx     USB-UART ↔ PC (PuTTY / plink / agent_uart.py)
```

**Cơ chế vận hành cốt lõi:** Ngắt `TIM2` (100 Hz / 10 ms) chỉ bật cờ thời gian chứ không đọc I2C trong ISR; vòng lặp chính trong [`src/main.c`](src/main.c) kiểm tra cờ để đọc MPU6050, bù offset, cập nhật bộ lọc bù và vẽ ký tự cắt lát (*sliced draw*, mỗi lượt 1 ký tự trong 4×24 ô) lên ILI9341 để giữ độ đáp ứng cho lệnh `USART1` (nhận theo ngắt `RXNE`).

---

## 1. Kiến trúc phần cứng, chân nối và xung nhịp (`src/` chuẩn)

> **Lưu ý nguồn sự thật:** Bảng chân nối và xung nhịp dưới đây lấy trực tiếp từ mã nguồn đang hoạt động trong `src/`. Một số tài liệu cũ trong `docs/Research/` hoặc `docs/Wiring_and_Pinouts.md` còn nhắc kiến trúc Dual-MCU STM32+ESP32 hoặc SPI1 18 MHz; các chi tiết đó đã được thay thế bởi `src/`.

| Thành phần | Tín hiệu module → chân STM32 | Cấu hình chuẩn trong `src/` |
|---|---|---|
| **MPU6050 (IMU)** | `SCL` → **PB6**, `SDA` → **PB7**, `AD0` → **GND**, `INT` không nối | I2C phần mềm open-drain, địa chỉ 7-bit **0x68**; dùng trễ `~5 µs` qua DWT `CYCCNT` (không phải chuẩn clock 100 Hz) |
| **W25Qxx (SPI Flash)** | `CS` → **PA4**, `SCK` → **PA5**, `MISO` → **PA6**, `MOSI` → **PA7** | `SPI1`, prescaler **/8** từ PCLK2 72 MHz → **9 MHz** danh định; `WP`/`HOLD` kéo mức logic phù hợp |
| **ILI9341 (TFT LCD)** | `CS` → **PB12**, `SCK` → **PB13**, `MOSI` → **PB15**, `PB14` input, `DC/RS` → **PB10**, `RESET` → **PB11** | `SPI2`, prescaler **/4** từ PCLK1 36 MHz → **9 MHz** danh định; chế độ ngang **320 × 240**, **RGB565** |
| **USB-UART (USART1)** | `PA9 (TX)` → `RXD` adapter, `PA10 (RX)` ← `TXD` adapter, `GND` ↔ `GND` | **115200 baud, 8-N-1**, không flow control; nhận lệnh kết thúc bằng CR, LF hoặc CRLF; phản hồi dùng CRLF |
| **ST-Link (SWD)** | `PA13` (**SWDIO**), `PA14` (**SWCLK**), `GND` ↔ `GND` | Nạp và debug tại `0x08000000` |

**An toàn nguồn và mức logic 3.3 V:** Các chân giao tiếp dùng mức logic 3.3 V. Không mặc định chân 3.3 V của mọi ST-Link đủ dòng nuôi đồng thời MCU, Flash và đèn nền TFT; kiểm tra datasheet từng module thực tế trước khi cấp 5 V vào chân VCC/LED. Khi board đã có nguồn riêng, USB-UART chỉ nối 3 dây **TX, RX, GND** chung.

### Thông số xung nhịp và thuật toán trong `src/`

| Tham số | Giá trị trong mã nguồn | Nguồn kiểm chứng |
|---|---|---|
| Clock hệ thống | **72 MHz** từ thạch anh ngoài **HSE 8 MHz** | [`system_stm32f10x.c`](third_party/stm32f10x/Libraries/CMSIS/CM3/DeviceSupport/ST/STM32F10x/system_stm32f10x.c) |
| Chu kỳ lấy mẫu IMU | **TIM2 100 Hz** (10 ms) đặt cờ cho vòng lặp chính | [`src/lib/drivers/display_system.c`](src/lib/drivers/display_system.c) |
| I2C mềm MPU6050 | Trễ bit-bang **~5 µs** (`Delay_us(5)` via DWT); phụ thuộc tốc độ GPIO và pull-up, **không phải 100 Hz** | [`src/lib/drivers/mpu6050.c`](src/lib/drivers/mpu6050.c), [`src/lib/drivers/system_time.c`](src/lib/drivers/system_time.c) |
| Nhận diện W25Qxx | `Flash_Init()` chấp nhận Winbond `0xEF 0x40`/`0x60` (dung lượng `0x14..0x18`) và `0x20 0x40 0x16`, kiểm tra `spi_error` | [`src/lib/drivers/flash_config.c`](src/lib/drivers/flash_config.c) |
| Bộ lọc bù Roll/Pitch | `alpha` mặc định **0.980**, dải hợp lệ **`0.50 .. 0.999`**; có hiệu chuẩn tĩnh 200 mẫu | [`src/lib/shared/imu_types.h`](src/lib/shared/imu_types.h), [`src/lib/shared/angle_filter.c`](src/lib/shared/angle_filter.c) |

Hiệu chuẩn (`CALIBRATE_IMU`), ghi Flash hoặc phục hồi cảm biến lỗi là các thao tác đồng bộ/blocking nên có thể làm chậm tạm thời nhịp vẽ màn hình và chu kỳ lấy mẫu; hệ thống không phải RTOS đảm bảo deadline cứng. Các giá trị 9 MHz của SPI1/SPI2 là tần số danh định tính từ cấu hình prescaler, không thay cho kết quả đo logic analyzer.

---

## 2. Thành viên và cấu trúc thư mục

Danh sách 5 thành viên khớp với `UART_SendGroupInfo()` trong [`src/lib/shared/uart_cmd.c`](src/lib/shared/uart_cmd.c) và [`src/lib/shared/imu_types.h`](src/lib/shared/imu_types.h):

| STT | Thành viên (`uart_cmd.c` / `imu_types.h`) | Phần phụ trách | Tệp mã nguồn chính |
|---|---|---|---|
| **TV1** | **Trần Đức Anh** (`Tran Duc Anh`) | Driver MPU6050, I2C mềm, đọc dữ liệu thô | [`src/lib/drivers/mpu6050.c`](src/lib/drivers/mpu6050.c) |
| **TV2** | **Trần Quốc Ân** (`Tran Quoc An`) | Hiệu chuẩn tĩnh 200 mẫu và bù offset 6 trục | [`src/lib/shared/calibration.c`](src/lib/shared/calibration.c) |
| **TV3** | **Nguyễn Duy Anh** (`Nguyen Duy Anh`) | Bộ lọc bù tính góc Roll/Pitch | [`src/lib/shared/angle_filter.c`](src/lib/shared/angle_filter.c) |
| **TV4** | **Trần Tuấn Anh** (`Tran Tuan Anh`) | Parser lệnh `USART1` và lưu/nạp cấu hình Flash | [`src/lib/shared/uart_cmd.c`](src/lib/shared/uart_cmd.c), [`src/lib/drivers/flash_config.c`](src/lib/drivers/flash_config.c) |
| **TV5** | **Trần Khắc Anh** (`Tran Khac Anh`) | Hệ thống thời gian, hiển thị ILI9341 và tích hợp | [`src/main.c`](src/main.c), [`src/lib/drivers/display_system.c`](src/lib/drivers/display_system.c), [`src/lib/drivers/ili9341.c`](src/lib/drivers/ili9341.c), [`src/lib/drivers/system_time.c`](src/lib/drivers/system_time.c) |

### Bản đồ thư mục công khai

- [`src/`](src/) — Mã nguồn firmware chuẩn (entry point: [`src/main.c`](src/main.c)).
- [`third_party/stm32f10x/`](third_party/stm32f10x/) — Thư viện SPL V3.5.0 & CMSIS V1.30 đóng gói sẵn để build portable không cần tải ngoài.
- [`tests/`](tests/) — Kiểm thử host CTest và bộ kiểm tra biên JEDEC/UART ([`tests/repro/check_flash_uart_boundaries.py`](tests/repro/check_flash_uart_boundaries.py)).
- [`scripts/`](scripts/) — Script kiểm thử tự động, nạp board và giao tiếp serial cho người dùng lẫn AI agent.
- [`docs/`](docs/) — Datasheet gốc, ghi chú nghiên cứu, hướng dẫn UART và báo cáo audit.
- **Thư mục cục bộ bị ignore (`.gitignore`):** `BTLVXL-Nhóm 1/`, `team_code`, `stm32_bringup/` và `workspace/` là bản làm việc nội bộ cục bộ, **không bắt buộc** cho quá trình build hay CI chuẩn.

---

## 3. Cài đặt môi trường & quản lý gói

Các công cụ kiểm thử và serial Python của dự án ([`scripts/agent_test.sh`](scripts/agent_test.sh) và [`scripts/agent_uart.py`](scripts/agent_uart.py)) chỉ sử dụng **thư viện chuẩn Python (Python stdlib)**, không cần `pip install` gói bên thứ ba.

### Ubuntu / Debian

```bash
sudo apt-get update && sudo apt-get install -y --no-install-recommends \
  build-essential cmake python3 \
  gcc-arm-none-eabi libnewlib-arm-none-eabi \
  stlink-tools putty putty-tools # --no-install-recommends tránh kéo gói thừa; putty-tools cung cấp plink
```

### Windows 10 / 11

- **Terminal Serial (`PuTTY` / `plink`):** Cài từ trang chủ [chiark PuTTY](https://www.chiark.greenend.org.uk/~sgtatham/putty/latest.html) hoặc qua WinGet:
  ```powershell
  winget install PuTTY.PuTTY # Cài cả putty.exe và plink.exe vào PATH
  ```
- **Driver USB-UART & Probe:** Cài đúng driver cho chip thực tế: **CH340/CH341**, **Silicon Labs CP2102**, **FTDI FT232** hoặc **ST-Link VCP/SWD** (xem thêm [`docs/WINDOWS_UART_SETUP_PUTTY_HERCULES.md`](docs/WINDOWS_UART_SETUP_PUTTY_HERCULES.md)).
- **Biên dịch CLI (tùy chọn trên Windows):** Cài ARM GNU Toolchain (`arm-none-eabi-gcc`) + CMake/Make để build qua [`Makefile`](Makefile) gốc với `third_party/stm32f10x/` mà không cần tải thư viện ST ngoài. Keil MDK chỉ là IDE tùy chọn bên ngoài nếu mở thư mục legacy `BTLVXL-Nhóm 1/`.

---

## 4. Runbook: Build và kiểm thử offline

### 1. Inspection / Kiểm tra công cụ và trạng thái repo

```bash
git status --short                        # --short xem thay đổi hiện có trước khi thao tác
command -v cc make cmake python3 bash     # Kiểm tra toolchain build host
command -v arm-none-eabi-gcc arm-none-eabi-size # Chỉ bắt buộc khi build firmware ARM
```

*Trạng thái khỏe mạnh:* In ra đường dẫn đầy đủ từng công cụ, không báo thiếu lệnh cần thiết.

### 2. Action / Biên dịch firmware ARM

```bash
make firmware # Biên dịch C99 + SPL trong third_party/stm32f10x/, KHÔNG nạp board
```

Đầu ra sinh tại `build/firmware/`: `firmware.elf`, `firmware.bin`, `firmware.hex` và `firmware.map`.

### 3. Verification / Kiểm thử host, biên giới hạn và kiểm tra chéo cho Agent

```bash
make test                                 # Một lệnh cho toàn bộ host CTest (4/4), kiểm tra biên Flash/UART và agent_uart --self-test
make test FIRMWARE=1                      # Thêm bước compile/link ARM (tương đương bash scripts/agent_test.sh --firmware)
python3 -B tests/repro/check_flash_uart_boundaries.py src # -B không ghi __pycache__; ép chạy slice JEDEC/ISR thật từ src/
arm-none-eabi-size build/firmware/firmware.elf            # Kiểm tra dung lượng text/data/bss sau khi build ARM
```

*Ranh giới bằng chứng (Host vs Hardware):*
- Bộ test CTest trên host dùng nhánh mô phỏng/stub cho phần cứng I2C/SPI/UART (`#else` trong các driver); pass trên host **không chứng minh** đường dây I2C/SPI, độ chính xác góc, timing thật hay độ bền Flash trên silicon.
- `check_flash_uart_boundaries.py src` biên dịch và vét cạn 16.777.216 tổ hợp JEDEC + ma trận kết thúc dòng CR/LF/CRLF trên đoạn mã nguồn thật từ `src/`, nhưng vẫn là kiểm thử cách ly trên host.
- Build ARM thành công (`--firmware`) chứng minh hợp lệ cú pháp/liên kết bộ nhớ cho Cortex-M3, không thay thế kiểm thử đo thực tế trên Blue Pill.

### 4. Rollback / Dọn dẹp thư mục build

```bash
make clean # Xóa build/ cục bộ; không chạm mã nguồn hay bộ nhớ Flash của STM32
```

---

## 5. Runbook: Nạp board và giao tiếp Serial (PuTTY & AI Agent)

### 1. Inspection / Kiểm tra kết nối phần cứng và quyền serial

```bash
st-info --probe             # Kiểm tra probe ST-Link và nhận diện chip STM32F103 trước khi nạp
ls -l /dev/serial/by-id/    # Xem tên thiết bị cố định của USB-UART trên Linux (hoặc /dev/ttyUSB* /dev/ttyACM*)
id                          # Kiểm tra user hiện tại đã thuộc nhóm dialout (Debian/Ubuntu) chưa
```

*Lưu ý quyền trên Linux:* Nếu thiếu quyền đọc/ghi serial, thêm tài khoản vào nhóm `dialout` (`sudo usermod -aG dialout "$USER"` rồi đăng xuất/đăng nhập lại) để giữ quyền chuẩn mà không cần mở quyền ghi toàn hệ thống lên thiết bị `/dev/tty*`.

### 2. Action / Nạp firmware và mở kênh điều khiển

**Nạp firmware xuống Flash nội STM32 (`0x08000000`):**

```bash
make flash # Tất cả trong một: build + probe ST-Link + nạp 0x08000000 (st-flash, tự fallback OpenOCD) + dò UART (thêm UART=1 để thử GET_ANGLE, PUTTY=1 để mở PuTTY, TARGET=hw-test cho test phần cứng)
```

**Kênh cho người dùng (Human — PuTTY / plink / PowerShell):**
- **Linux GUI:** `bash scripts/linux/launch_putty.sh /dev/ttyUSB0` (tự cấu hình `115200,8,n,1,N`).
- **Linux CLI (`plink`):** `bash scripts/linux/launch_putty.sh --cli /dev/ttyUSB0` (`--cli` gọi `plink -serial ... -sercfg 115200,8,n,1,N`).
- **Windows:** Chạy [`scripts\windows\launch_putty.bat`](scripts/windows/launch_putty.bat) (tự dò COM port và mở PuTTY), hoặc dùng script PowerShell thuần stdlib [`scripts\windows\serial_terminal.ps1`](scripts/windows/serial_terminal.ps1).
- *Cấu hình PuTTY thủ công:* `115200` baud, `8` data bits, `None` parity, `1` stop bit, `None` flow control; bật **Local echo = Force on** và **Local line editing = Force on** (vì firmware nhận âm thầm theo dòng và không echo từng ký tự).

**Kênh cho AI Agent (Non-interactive CLI, Python stdlib-only):**

```bash
python3 -B scripts/agent_uart.py --probe-ports                                                # --probe-ports liệt kê cổng serial hiện có (chỉ đọc)
python3 -B scripts/agent_uart.py --port /dev/ttyUSB0 --command GET_ANGLE --json               # --json xuất phản hồi dạng cấu trúc cho agent
python3 -B scripts/agent_uart.py --port /dev/ttyUSB0 --command CALIBRATE_IMU --allow-write --timeout 5 --json # --allow-write bắt buộc cho lệnh ghi/hiệu chuẩn
```

> **Cổng bảo vệ ghi (`--allow-write`):** [`scripts/agent_uart.py`](scripts/agent_uart.py) chặn các lệnh làm thay đổi trạng thái MCU/Flash (`CALIBRATE_IMU`, `SET_FILTER_ALPHA:...`) trừ khi truyền rõ `--allow-write`. Với `CALIBRATE_IMU` (lấy 200 mẫu ~2 s + ghi Flash), dùng `--timeout 5` trở lên.

### 3. Verification / Kiểm tra phản hồi giao thức UART

Kiểm tra tool agent offline trước khi nối board (qua PTY cục bộ):

```bash
python3 -B scripts/agent_uart.py --self-test # --self-test kiểm thử parser/framing qua pseudo-terminal không cần phần cứng
```

Bảng lệnh UART chuẩn (firmware chấp nhận **CR, LF hoặc CRLF** kết thúc lệnh; phản hồi trả về dùng **CRLF**; góc chỉ trả khi hỏi `GET_ANGLE`, không tự stream):

| Lệnh gửi (`uart_cmd.c`) | Tham số / Điều kiện | Phản hồi thành công / Trạng thái phụ |
|---|---|---|
| `INFO_REQUEST` hoặc `INFO_REQUEST:GROUP` | Không | `GROUP_NAME: Nhom 1`, `MEMBERS: ...`, `PROJECT_NAME`, `HARDWARE_USED`, `SOFTWARE_USED`, `DATE` *(chỉ chứng minh đường UART sống, không thay cho kiểm tra MPU6050/SPI)* |
| `GET_ANGLE` | Không | `ROLL:<độ>\r\nPITCH:<độ>\r\n`; hoặc `ERROR:ANGLE_NOT_READY` |
| `CALIBRATE_IMU` | Đặt board nằm yên, mặt phẳng ngang (`Z ≈ +1 g`), thu 200 mẫu (~2 s) | `CALIBRATE:OK`, `ACC_OFFSET:...`, `GYRO_OFFSET:...`; lỗi I2C báo `ERROR:CALIBRATE_FAILED`; lỗi Flash báo `ERROR:FLASH_SAVE_FAILED;CALIBRATION_IN_RAM` |
| `SET_FILTER_ALPHA:0.980` | `0.50 <= alpha <= 0.999` | Chưa hiệu chuẩn: `STATUS:RAM_ONLY;CALIBRATE_TO_SAVE` + `FILTER_ALPHA:...`; đã hiệu chuẩn: lưu W25Qxx rồi trả `FILTER_ALPHA:...` (hoặc `ERROR:FLASH_SAVE_FAILED`); sai ngưỡng trả `ERROR:INVALID_ALPHA` |

### 4. Rollback / Thoát terminal và khôi phục firmware

Đóng cửa sổ PuTTY, nhấn `Ctrl+C` trong `plink`, hoặc nạp lại một bản `.bin` đã xác nhận hoạt động tốt:

```bash
st-flash --reset write /path/to/known-good.bin 0x08000000 # --reset khởi động lại MCU ngay sau khi nạp xong Flash nội
```

*Lưu ý giới hạn rollback:* Lệnh trên chỉ ghi đè bộ nhớ Flash nội của STM32, **không hoàn tác** sector cấu hình đang lưu trong W25Qxx. Lưu lại bản `.bin` và thông số offset trước khi thử nghiệm; không chạy xóa toàn bộ chip (mass erase) như bước xử lý mặc định.

---

## 6. Datasheet gốc, tài liệu tham khảo và quản trị dự án

### Datasheet phần cứng (`docs/DataSheet/`)

| Tài liệu | Phiên bản / Mã kiểm chứng | Phạm vi sử dụng |
|---|---|---|
| [`docs/DataSheet/stm32f103c8.pdf`](docs/DataSheet/stm32f103c8.pdf) | STMicroelectronics **DS5319 Rev 20** (114 trang) | Thông số điện, pinout và giới hạn chân STM32F103x8/xB |
| [`docs/DataSheet/stm32f103_rm0008_reference_manual.pdf`](docs/DataSheet/stm32f103_rm0008_reference_manual.pdf) | STMicroelectronics **RM0008 Rev 21** (1136 trang) | Thanh ghi RCC, GPIO, TIM2, SPI1/SPI2, USART1 |
| [`docs/DataSheet/mpu-6050_datasheet_v3_4.pdf`](docs/DataSheet/mpu-6050_datasheet_v3_4.pdf) | InvenSense **PS-MPU-6000A-00 Rev 3.4**, 08/19/2013 (52 trang) | Đặc tính điện và cảm biến MPU-6000/MPU-6050 |
| [`docs/DataSheet/RM-MPU-6000A.pdf`](docs/DataSheet/RM-MPU-6000A.pdf) | InvenSense **RM-MPU-6000A-00 Rev 4.0**, 03/09/2012 (47 trang) | Bản đồ thanh ghi MPU-6050 (`WHO_AM_I`, `PWR_MGMT_1`, `ACCEL_XOUT_H`) |
| [`docs/DataSheet/w25q64fv.pdf`](docs/DataSheet/w25q64fv.pdf) | Winbond **W25Q64FV Revision L**, Oct 07, 2013 (89 trang) | Tập lệnh SPI NOR Flash, JEDEC ID, xóa sector 4 KB & page program |
| [`docs/DataSheet/esp32_datasheet_en.pdf`](docs/DataSheet/esp32_datasheet_en.pdf) | Espressif **v5.3** (78 trang) — *chỉ lưu tham khảo lịch sử* | Thuộc ghi chú nghiên cứu Dual-MCU cũ, **không** dùng trong firmware `src/` |

### Tài liệu nghiên cứu, vận hành và kiểm toán

- **Nghiên cứu kỹ thuật (`docs/Research/`):** [`STM32.md`](docs/Research/STM32.md), [`MPU6050.md`](docs/Research/MPU6050.md), [`W25Q64_Flash.md`](docs/Research/W25Q64_Flash.md), [`UART_Protocol_and_Framing.md`](docs/Research/UART_Protocol_and_Framing.md), [`TFT_ILI9341_SPI.md`](docs/Research/TFT_ILI9341_SPI.md). *(Cảnh báo: mọi ghi chú cũ trong thư mục này hoặc tài liệu nối dây về cấu hình Dual-MCU hay SPI1 18 MHz đều bị thay thế bởi chân/clock thực tế trong `src/`.)*
- **Vận hành & Audit:**
  - [`docs/Audit_2026-11-10_Flash_UART_MPU6050.md`](docs/Audit_2026-11-10_Flash_UART_MPU6050.md) — Báo cáo kiểm toán JEDEC Flash, CR/LF UART và đối chiếu driver MPU6050.
  - [`docs/UART_COMMANDS_REFERENCE.md`](docs/UART_COMMANDS_REFERENCE.md) — Tham chiếu lệnh UART.
  - [`docs/WINDOWS_UART_SETUP_PUTTY_HERCULES.md`](docs/WINDOWS_UART_SETUP_PUTTY_HERCULES.md) — Hướng dẫn cài đặt PuTTY và Hercules trên Windows.
  - [`docs/LINUX_UART_SETUP_HERCULES_EQUIVALENTS.md`](docs/LINUX_UART_SETUP_HERCULES_EQUIVALENTS.md) — Công cụ UART tương đương trên Linux.

### CI, công cụ tự động và chính sách cộng đồng

- **CI & Tự động hóa:** [`.github/workflows/ci.yml`](.github/workflows/ci.yml), [`.github/dependabot.yml`](.github/dependabot.yml), [`Makefile`](Makefile), [`scripts/agent_test.sh`](scripts/agent_test.sh), [`scripts/agent_uart.py`](scripts/agent_uart.py), [`scripts/linux/launch_putty.sh`](scripts/linux/launch_putty.sh), [`scripts/windows/launch_putty.bat`](scripts/windows/launch_putty.bat), [`scripts/windows/serial_terminal.ps1`](scripts/windows/serial_terminal.ps1).
- **Quản trị & Đóng góp:** [`.github/CODEOWNERS`](.github/CODEOWNERS), [`.github/ISSUE_TEMPLATE/bug_report.yml`](.github/ISSUE_TEMPLATE/bug_report.yml), [`.github/ISSUE_TEMPLATE/feature_request.yml`](.github/ISSUE_TEMPLATE/feature_request.yml), [`.github/PULL_REQUEST_TEMPLATE.md`](.github/PULL_REQUEST_TEMPLATE.md), [`CONTRIBUTING.md`](CONTRIBUTING.md), [`SECURITY.md`](SECURITY.md), [`CODE_OF_CONDUCT.md`](CODE_OF_CONDUCT.md).

Giữ nguyên toàn bộ thông báo bản quyền và điều khoản đi kèm trong [`third_party/stm32f10x/Release_Notes.html`](third_party/stm32f10x/Release_Notes.html) cùng các thành phần bên thứ ba. Tài liệu README này chỉ mô tả kỹ thuật và không cấp thêm giấy phép sử dụng mã nguồn.
