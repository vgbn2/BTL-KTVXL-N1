# Project MPU6050 - sau khi sua loi ghep nhom

## Mo project

Mo MDK/Calibration_An.uvprojx. Neu Keil dang mo ban cu, dong va mo lai project de nap danh sach file moi. Ban goc truoc khi sua nam trong backup_before_fix; khong them cac file backup vao Target.

Project dung C99, Keil ARMCC 5, CMSIS va SPL. Thu vien SPL da duoc chep vao Libraries/SPL de tranh phu thuoc duong dan tuyet doi. Giu goi Keil.STM32F1xx_DFP 2.1.0 va CMSIS 4.5.0 dang cai tren may.

## So do chan de xuat

Day la so do moi theo lua chon chua chot day cua nhom. Doi chieu ma module va nguon thuc te truoc khi noi.

| Thiet bi | Chan module | STM32F103C8 |
|---|---|---|
| MPU6050 | SCL | PB6 |
| MPU6050 | SDA | PB7 |
| MPU6050 | AD0 | GND, dia chi 0x68 |
| MPU6050 | INT, XDA, XCL | Khong su dung |
| W25Qxx | CLK | PA5 SPI1_SCK |
| W25Qxx | DO/MISO | PA6 SPI1_MISO |
| W25Qxx | DI/MOSI | PA7 SPI1_MOSI |
| W25Qxx | CS | PA4 |
| W25Qxx | WP, HOLD neu co | Keo len 3.3 V |
| ILI9341 | SCK | PB13 SPI2_SCK |
| ILI9341 | SDI/MOSI | PB15 SPI2_MOSI |
| ILI9341 | CS | PB12 |
| ILI9341 | DC/RS | PB10 |
| ILI9341 | RESET | PB11 |
| ILI9341 | SDO/MISO | Khong su dung |
| USB-UART | RX | PA9 USART1_TX |
| USB-UART | TX | PA10 USART1_RX |
| ST-Link | SWDIO, SWCLK | PA13, PA14 |

Tat ca chung GND. I2C SDA/SCL can dien tro keo len 3.3 V (kiem tra module da co chua). Nguon cap LCD va chan LED theo module cu the; khong lay dong den nen truc tiep tu GPIO. Khong dua muc logic 5 V vao bus da thiet ke 3.3 V. Flash du kien ho W25Q 3.3 V, JEDEC type 0x40/0x60, capacity 0x14..0x18. Neu module khac, doi chieu datasheet truoc khi sua kiem tra ID.

MPU6050 hien dung I2C mem tren GPIO SPL (open-drain); khong dung HAL va khong can hi2c1. Delay_us dung DWT; Delay_ms dung SysTick. Cac ham cho phai goi tu main, khong tu ngat. TIMER2 lay clock thuc te tu RCC de tao chu ky 10 ms; thach anh ngoai du kien 8 MHz nhu cau hinh CMSIS.

## Cac thay doi chinh

- Sua include guard calibration.h; dong bo prototype theo imu_types.h.
- Viet lai MPU6050 SPL: dung ten bien, timeout, ID 0x68, SMPLRT_DIV=9, chuyen 6 truc sang g va do/giay.
- Bo cac ten ham khong ton tai trong main/display; ghep Calibration_Apply -> Angle_Update -> Angle_Get.
- Them board.c/h cho thoi gian, them ili9341.c/h cho LCD SPI2 va font chu/so toi thieu.
- LCD ve tung ky tu trong vong lap de giam thoi gian chan lay mau. Mau bi tre hon 50 ms se khoi tao lai goc; khong gia dinh dt luon 0.01 khi vong lap bi chan.
- Cam bien loi se duoc thu khoi tao lai. UART va man hinh van duoc phuc vu.
- UART nhan theo LF hoac CRLF, co hang doi 3 lenh cho, bao tran/loi; khong dung IDLE lam ket thuc lenh.
- UART giu dau am cua so nho, hien alpha/offset 3 chu so thap phan va kiem tra toan bo chuoi alpha.
- Flash co timeout, kiem tra WEL, CRC32 that, doc lai sau ghi. Chu y: sector 0 duoc danh rieng cho config va se bi xoa khi luu.
- Chi nap offset tu Flash khi magic, CRC va gia tri deu hop le.
- Hieu chuan xong/reset thoi gian sau luu Flash de khong tich phan khoang dung voi dt sai.
- Chu thich C/H trong project va ban SPL local da doi sang // khong dau. Giay phep cua thu vien van duoc giu noi dung. RTE_Components.h co the duoc Keil tu sinh lai voi chu thich khoi khi build.

## Lenh UART

115200 baud, 8 data bits, no parity, 1 stop bit. Moi lenh ket thuc bang LF hoac CRLF.

- INFO_REQUEST hoac INFO_REQUEST:GROUP: thong tin Nhom 1 va thiet bi. DATE hien la ngay bien dich (__DATE__), chua la ngay thoi gian thuc do he thong chua co RTC/lenh cap nhat ngay. Neu giang vien yeu cau dung ngay gui, nhom can them nguon thoi gian.
- CALIBRATE_IMU: dat nam ngang, Z huong len, giu yen khoang 2 giay. Chi bao CALIBRATE:OK khi luu Flash thanh cong. Neu Flash loi, offset moi van o RAM va phan hoi noi ro.
- SET_FILTER_ALPHA:0.98: pham vi 0.50..0.999. Chua hieu chuan thi alpha chi duoc giu RAM, phan hoi STATUS:RAM_ONLY; hieu chuan thanh cong sau do se luu ca offset va alpha. Sau khi da hieu chuan, doi alpha se luu Flash.
- GET_ANGLE: tra ROLL/PITCH khi da co mau hop le; neu chua co hoac cam bien loi thi ERROR:ANGLE_NOT_READY.

## Ket qua kiem tra

1. GCC kiem tra cu phap cac file USER va SPL: thanh cong.
2. tests/test_math.c kiem tra hieu chuan, giu offset khi doc loi, du lieu NaN, alpha va goc Roll qua bien +/-180: PASS.
3. Da build va link bang Keil uVision 5.17 / ARMCC 5.06: 0 loi, 1 canh bao calibration.c thieu xuong dong cuoi file. Code=22546, RO-data=1066, RW-data=124, ZI-data=2268 byte. Da tao MDK/Objects/Calibration_An.hex.
4. Sau lan build do da bo sung xuong dong cuoi calibration.c, tang stack startup tu 1024 len 2048 byte, va doi lai chu thich RTE_Components.h do Keil sinh. Yeu cau build lai bi tu choi, nen HEX/AXF hien co la ban TRUOC cac thay doi cuoi. Can Rebuild trong Keil truoc khi nap ban moi.
5. Chua nap board, chua xac nhan timing I2C/LCD/SPI, phan hoi UART hay luu qua tat nguon tren phan cung that. Build thanh cong khong tu dong chung minh day noi hay LCD hoat dong.

## Can kiem tra tren board

- Build lai, nap HEX moi, kiem tra INFO_REQUEST.
- Doc cam bien khi nam ngang, sau hieu chuan Acc gan (0,0,1), Gyro gan (0,0,0).
- Nghieng tung truc, doi chieu chieu goc tren UART va LCD.
- Gui alpha hop le/sai/dinh kem chu; kiem tra so am nho nhu -0.5 va alpha 0.98.
- Hieu chuan, doi alpha, tat/bat nguon va kiem tra cau hinh.
- Rut cam bien de kiem tra bao loi/phuc hoi; Flash loi khong duoc bao luu thanh cong.
- Do thoi gian vong doc khi LCD/UART hoat dong; TIMER tao nhip 10 ms nhung tac vu chan van co the lam tre.

## Gioi han con lai

Hieu chuan mot tu the chua tu phat hien rung/tu the sai va chua bu sai so nhiet do. Bo loc Roll/Pitch dang dung mo hinh don gian cua de, phu hop demo goc nghieng, khong thay the thuat toan tu the 3D day du khi quay ket hop manh. Cau hinh Flash luu mot sector; mat nguon giua xoa/ghi co the mat cau hinh, lan sau se dung mac dinh neu CRC sai. Driver LCD SPI khong doc ID, nen chi co the bao loi truyen SPI, khong khang dinh man hinh da nhan lenh.
