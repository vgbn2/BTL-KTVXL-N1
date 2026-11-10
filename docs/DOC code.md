

|  |  |
| ----- | :---- |
|  |  |
|  |  |
|  |  |
|  |  |

\#ifdef CALIBRATION\_H  
\#define CALIBRATION\_H  
\#include "imu\_types.h"  
\#include \<stdbool.h\>

int Calibration\_Run (void); // Lay 200 mau tinh, tinh bu offset 6 truc MPU va tra ve ma loi  
void Calibration\_Apply (IMU\_Data\_t \*data); // tru truc tiep 6 gia tri offset vao du lieu cua moi chu ki do  
void Calibration\_GetOffset (IMU\_Offset\_t \*offset); //getter de lay cac offdet ra Flash  
void Calibration\_SetOffset(const IMU\_Offset\_t \*offset);//setter nap offset tu Flash vao RAM  
bool Calibration\_IsCalibrated(void); // check hieu chuan thong so  
 \#endif  
 

\#include "calibration.h"  
\#include "mpu6050.h"  
\#include \<stddef.h\>

extern void Delay\_ms(uint32\_t time\_ms); //ham tre lay o file khac  
//dong goi du lieu va khoi tao ban dau  
static IMU\_Offset\_t current\_offset={0};  
static bool is\_calibrated \= false;

int Calibration\_Run (void) { // Lay 200 mau tinh, tinh bu offset 6 truc MPU va tra ve ma loi  
	IMU\_Data\_t data; // luu du lieu tho moi vong lap\]  
	// cong don gia tri 6 truc  
	float sum\_ax \= 0.0f;  
	float sum\_ay \= 0.0f;  
	float sum\_az \=0.0f;  
	  
	float sum\_gx \= 0.0f;  
	float sum\_gy \= 0.0f;  
	float sum\_gz \= 0.0f;  
	  
	int status; // nhan ma tra ve de kiem tra  
	uint32\_t i;  
	for(i=0U;i\<IMU\_CALIB\_SAMPLES;i++){ // kiem tra loi trong qua trinh lay mau qua 200 vong lap  
		status \= MPU6050\_Read(\&data);// nhan ma tu ham  
		if(status \!= IMU\_OK) {// kiem tra xem dung chua  
			return status;  
		}  
		// lay tong mau do de chia trung binh cong  
	 sum\_ax \= sum\_ax \+ data.ax\_g;  
        sum\_ay \= sum\_ay \+ data.ay\_g;  
        sum\_az \= sum\_az \+ data.az\_g;

        sum\_gx \= sum\_gx \+ data.gx\_dps;  
        sum\_gy \= sum\_gy \+ data.gy\_dps;  
        sum\_gz \= sum\_gz \+ data.gz\_dps;  
		if(i\<IMU\_CALIB\_SAMPLES-1U){ // delay 10ms trong 199m lan lay mau  
			Delay\_ms(10U);  
		}  
	}  
	// chia trung binh cong gia toc  
	current\_offset.accel\_offset\_x \= sum\_ax/IMU\_CALIB\_SAMPLES;  
	current\_offset.accel\_offset\_y \= sum\_ay/IMU\_CALIB\_SAMPLES;  
	current\_offset.accel\_offset\_z \= sum\_az/IMU\_CALIB\_SAMPLES-1.0f;// tru truc z 1.0g  
	// chia trung binh cong van toc goc  
	current\_offset.gyro\_offset\_x \= sum\_gx/IMU\_CALIB\_SAMPLES;  
	current\_offset.gyro\_offset\_y \= sum\_gy/IMU\_CALIB\_SAMPLES;  
	current\_offset.gyro\_offset\_z \= sum\_gz/IMU\_CALIB\_SAMPLES;  
	is\_calibrated \= true; // tin offset da xong  
	return IMU\_OK;  
}

void Calibration\_Apply(IMU\_Data\_t \*data){// tru truc tiep 6 gia tri offset vao du lieu cua moi chu ki do  
	// kiem tra con tro rong va hieu chinh  
	if(data==NULL) return;  
	if(is\_calibrated==false) return;  
	// lay du lieu thuc  
	data-\>ax\_g \= data-\>ax\_g \- current\_offset.accel\_offset\_x;  
    data-\>ay\_g \= data-\>ay\_g \- current\_offset.accel\_offset\_y;  
    data-\>az\_g \= data-\>az\_g \- current\_offset.accel\_offset\_z;

    data-\>gx\_dps \= data-\>gx\_dps \- current\_offset.gyro\_offset\_x;  
    data-\>gy\_dps \= data-\>gy\_dps \- current\_offset.gyro\_offset\_y;  
    data-\>gz\_dps \= data-\>gz\_dps \- current\_offset.gyro\_offset\_z;  
}  
void Calibration\_GetOffset(IMU\_Offset\_t \*offset){//getter de lay cac offdet ra Flash  
	if (offset==NULL) return;  
	\*offset \= current\_offset; //lay ban sao cua offset ra ngoai cho FLASH  
}  
void Calibration\_SetOffset(const IMU\_Offset\_t \*offset){//setter nap offset tu Flash vao RAM  
	if (offset==NULL) return;  
	current\_offset=\*offset; // nhan du lieu offset tu FLSAH  
	is\_calibrated=true;  
}  
bool Calibration\_IsCalibrated(void){// check hieu chuan thong so  
	return is\_calibrated;  
}

|  |  |
| ----- | :---- |
|  |  |
|  |  |
|  |  |
|  |  |

|  |  |
| ----- | :---- |
|  |  |
|  |  |
|  |  |
|  |  |

\#include "mpu6050.h"  
\#include "imu\_types.h"  
\#include \<stdint.h\>  
\#include \<string.h\>  
\#include "stm32f1xx\_hal.h"  
extern I2C\_HandleTypeDef hi2c1;

//mem write check  
static IMU\_Status\_t reg\_write(uint8\_y reg,uint8\_t data)  
{  
    if(HAL\_I2C\_Mem\_Write(  
                \&hi2c1,  
                (uint16)(MPU6050\_I2C\_ADDR \<\< 1,  
                reg,I2C\_MEMADD\_SIZE\_8BIT,  
                \&val,1,50)\!=HAL\_OK)

    return IMU\_ERR\_I2C;  
    return IMU\_OK;  
}  
//mem read check  
static IMU\_Status\_t reg\_read(uint8\_t reg,uint8\_t \*buf,uint16\_t len)  
{  
    if(HAL\_I2C\_Mem\_Read(  
                \&hi2c1,  
                (uint16)(MPU6050\_I2C\_ADDR \<\< 1,  
                reg,I2C\_MEMADD\_SIZE\_8BIT,  
                buffer,len,50)\!=HAL\_OK)

    return IMU\_ERR\_I2C;  
    return IMU\_OK;  
}

IMU\_Status\_t MPU6050\_Init(void)  
{  
    uint\_8 who=0;  
    IMU\_Status\_t ret;  
    //WHO AM I register, register 0x75u, chi de doc register 0x68U  
    //MPU6050 WHO\_AM\_I\_VAL  
    ret \=reg\_read(MPU6050\_REG\_WHO\_AM\_I,\&who,1);  
      
    if(\!IMU\_OK)return ret;  
    if(who\!= MPU6050\_WHO\_AM\_I\_6500 && who\!=MPU6050\_WHO\_AM\_I\_VAL)  
      
    return IMU\_ERR\_DEVICE\_ID;

    //reset toan bo thiet bi  
    //POWER\_MANAGEMENT\_1 bit 7 la reset (4.30 reference cua mpu 6050\)  
    //day la cong doan tat  
    ret \=reg\_write(MPU6050\_REG\_POWER\_MANAGEMENT\_1,0x80); //7 bit-\>hex=0x80   
    if(\!IMU\_OK)return ret;  
    HAL\_Delay(100);

    //day la cong doan bat  
    ret \=reg\_write(MPU6050\_REG\_POWER\_MANAGEMENT\_1,0x01);  
    if(\!IMU\_OK)return ret;  
    HAL\_Delay(10);  
      
    //dat tan so lay mau( sample rate)  
    //  
    ret \=reg\_write(MPU6050\_REG\_SAMPLE\_RATE\_DIVIDER,0x19);//register thap phan 25, hex 19, 4.2 ref  
    if(\!IMU\_OK)return ret;  
      
    //bo loc thong thap  
    //config register  
    ret \=ret\_write(MPU6050\_REG\_CONFIG,0x03);   
    if(\!IMU\_OK)return ret;

    //config cam bien gia toc  
    ret \=reg\_write(MPU6050\_REG\_ACCELERATION\_CONFIG,0x00);   
    if(\!IMU\_OK)return ret;  
    

    //config cam bien con quay  
    ret \=reg\_write(MPU6050\_REG\_GYRO\_CONFIG,0x00);  
    if(\!IMU\_OK)return ret;  
      
    //bật register  
    ret=reg\_write(MPU6050\_REG\_INT\_ENABLE,0x01);   
    if(\!IMU\_OK)return ret;

    return IMU\_OK;  
}

IMU\_Status\_t MPU6050\_Read(IMU\_Data\_t \*data)  
{  
    //ko co du lieu \-\>baos thao so loi  
    if(data==NULL)return IMU\_ERR\_INVALID\_PARAM;  
    uint8\_t raw\[14\];  
    IMU\_status\_t ret=reg\_read(MPU6050\_REG\_ACCELEROMETER\_XOUT\_HIGH\_BYTE,raw,14);  
    if(ret\!=IMU\_OK) return ret;

    data-\>ax\_g \=bytes\_to\_i16(raw\[0\],raw\[1\])/MPU6050\_ACCELEROMETER\_SENSITIVITY;  
    data-\>ax\_g \=bytes\_to\_i16(raw\[2\],raw\[3\])/MPU6050\_ACCELEROMETER\_SENSITIVITY;  
    data-\>ax\_g \=bytes\_to\_i16(raw\[4\],raw\[5\])/MPU6050\_ACCELEROMETER\_SENSITIVITY;  
      
    data-\>gx\_dps=bytes\_to\_i16(raw\[8\],raw\[9\])/MPU6050\_GYRO\_SENSITIVITY;  
    data-\>gx\_dps=bytes\_to\_i16(raw\[10\],raw\[11\])/MPU6050\_GYRO\_SENSITIVITY;  
    data-\>gx\_dps=bytes\_to\_i16(raw\[12\],raw\[13\])/MPU6050\_GYRO\_SENSITIVITY;

    return IMU\_OK;  
}  
int MPU6050\_ReadData(MPU6050\_DATA \*imu)  
{  
    if(imu \==NULL)return (int)IMU\_ERR\_INVALID\_PARAM;  
    uint8\_t raw\[14\];  
    IMU\_status\_t ret=reg\_read(MPU6050\_REG\_ACCELEROMETER\_XOUT\_HIGH\_BYTE,raw,14);  
      
    if(reg\_read(MPU6050\_REG\_ACCELEROMETER\_XOUT\_HIGH\_BYTE,raw,14)\!=IMU\_OK);  
    return (int)IMU\_ERR\_I2C;

    imu-\>ax=bytes\_to\_i16(raw\[0\],raw\[1\]);  
    imu-\>ay=bytes\_to\_i16(raw\[2\],raw\[3\]);  
    imu-\>az=bytes\_to\_i16(raw\[4\],raw\[5\]);  
    //bo 67  
    imu-\>gx=bytes\_to\_i16(raw\[8\],raw\[9\]);  
    imu-\>gx=bytes\_to\_i16(raw\[10\],raw\[11\]);  
    imu-\>gx=bytes\_to\_i16(raw\[12\],raw\[13\]);  
      
    return (int)IMU\_OK;  
}

\#ifndef MPU6050\_H  
\#define MPU6050\_H

\#include "imu\_types.h"

//dia chi register ung voi chucnang cua mpu  
\#define MPU6050\_REG\_SAMPLE\_RATE\_DIVIDER                     0x19U   //SMPLRT\_DIV, 4.2 ref  
\#define MPU6050\_REG\_CONFIG                                  0x1AU   //4.3 ref  
\#define MPU6050\_REG\_GYRO\_CONFIG                             0x1BU   //4.4 ref  
\#define MPU6050\_REG\_ACCELERATION\_CONFIG                     0x1CU   //4.5 ref  
\#define MPU6050\_REG\_INT\_STATUS                              0x3AU  
\#define MPU6050\_REG\_INT\_ENABLE                              0x38U   //4.16 ref    
\#define MPU6050\_REG\_ACCELEROMETER\_XOUT\_HIGH\_BYTE            0x3BU   //4.18 ref  
\#define MPU6050\_REG\_ACCELEROMETER\_YOUT\_HIGH\_BYTE            0x3DU   //4.18 ref  
\#define MPU6050\_REG\_ACCELEROMETER\_ZOUT\_HIGH\_BYTE            0x3FU   //4.18 ref  
\#define MPU6050\_REG\_POWER\_MANAGEMENT\_1                      0x6BU  
\#define MPU6050\_REG\_WHO\_AM\_I                                0x75U

\#define MPU6050\_I2C\_ADDR                                    0x68U  
\#define MPU6050\_WHO\_AM\_I\_VAL                                0x68U  
\#define MPU6050\_WHO\_AM\_I\_6500                               0x70U

//cam bien gia toc va con quay, se dung  
\#define MPU6050\_ACCELEROMETER\_SENSITIVITY                  16384.0f  
\#define MPU6050\_GYRO\_SENSITIVITY                            131.0f

typedef struct{  
    int16\_t ax,ay,az;    //acce xyz  
    int16\_t gx,gy,gz;   //gyro xyz  
}MPU6050\_DATA;

                                 
IMU\_Status\_t MPU6050\_init(void);  
//init MPU6050, 100hz tan so quet,+-2g accel, \+-250 gyro

IMU\_Status\_t MPU6050\_Read(void);  
//su dung boi calibration , doc 14byte

int MPU6050\_ReadData(MPU6050\_DATA \*imu);

\#endif /\* MPU6050\_H \*/

\#include "angle\_filter.h"  
\#include "imu\_types.h"  
\#include \<math.h\>

/\* Chuyen doi tu radian sang do \*/  
\#define ANGLE\_RAD\_TO\_DEG 57.2957795f

/\* Bien noi bo cua module \*/  
static float roll\_deg \= 0.0f;  
static float pitch\_deg \= 0.0f;  
static float filter\_alpha \= IMU\_DEFAULT\_ALPHA;

/\* Danh dau da co goc khoi tao tu Accelerometer hay chua \*/  
static bool has\_initial\_angle \= false;

/\* Khoi tao bo loc \*/  
void Angle\_Init(float alpha)  
{  
    roll\_deg \= 0.0f;  
    pitch\_deg \= 0.0f;

    has\_initial\_angle \= false;

    if (alpha \>= IMU\_ALPHA\_MIN && alpha \<= IMU\_ALPHA\_MAX)  
    {  
        filter\_alpha \= alpha;  
    }  
    else  
    {  
        filter\_alpha \= IMU\_DEFAULT\_ALPHA;  
    }  
}

/\* Cap nhat goc Roll va Pitch \*/  
void Angle\_Update(const IMU\_Data\_t \*data, float dt\_s)  
{  
    float acc\_roll\_deg;  
    float acc\_pitch\_deg;

    /\* Kiem tra du lieu dau vao \*/  
    if ((data \== NULL) \|\| \!(dt\_s \> 0.0f))  
    {  
        return;  
    }

    /\* Tinh goc Roll tu Accelerometer \*/  
    acc\_roll\_deg \= atan2f(data-\>ay\_g, data-\>az\_g)  
                   \* ANGLE\_RAD\_TO\_DEG;

    /\* Tinh goc Pitch tu Accelerometer \*/  
    acc\_pitch\_deg \= atan2f(  
                        \-data-\>ax\_g,  
                        sqrtf(data-\>ay\_g \* data-\>ay\_g  
                            \+ data-\>az\_g \* data-\>az\_g)  
                    ) \* ANGLE\_RAD\_TO\_DEG;

    /\* Khoi tao goc ban dau tu Accelerometer \*/  
    if (\!has\_initial\_angle)  
    {  
        roll\_deg \= acc\_roll\_deg;  
        pitch\_deg \= acc\_pitch\_deg;

        has\_initial\_angle \= true;  
    }  
    else  
    {  
        /\* Bo loc bu cho goc Roll \*/  
        roll\_deg \= filter\_alpha  
                   \* (roll\_deg \+ data-\>gx\_dps \* dt\_s)  
                   \+ (1.0f \- filter\_alpha) \* acc\_roll\_deg;

        /\* Bo loc bu cho goc Pitch \*/  
        pitch\_deg \= filter\_alpha  
                    \* (pitch\_deg \+ data-\>gy\_dps \* dt\_s)  
                    \+ (1.0f \- filter\_alpha) \* acc\_pitch\_deg;  
    }  
}

/\* Lay gia tri goc hien tai \*/  
void Angle\_Get(Angle\_Data\_t \*angle)  
{  
    if (angle \== NULL)  
    {  
        return;  
    }

    angle-\>roll\_deg \= roll\_deg;  
    angle-\>pitch\_deg \= pitch\_deg;  
}

/\* Thay doi he so loc alpha \*/  
int Angle\_SetAlpha(float alpha)  
{  
    if (\!(alpha \>= IMU\_ALPHA\_MIN && alpha \<= IMU\_ALPHA\_MAX))  
    {  
        return IMU\_ERR\_INVALID\_PARAM;  
    }

    filter\_alpha \= alpha;

    return IMU\_OK;  
}

/\* Lay he so loc alpha hien tai \*/  
float Angle\_GetAlpha(void)  
{  
    return filter\_alpha;  
}

\#ifndef ANGLE\_FILTER\_H  
\#define ANGLE\_FILTER\_H

\#include "imu\_types.h"

/\* Khoi tao bo loc Complementary Filter \*/  
void Angle\_Init(float alpha);

/\* Cap nhat goc Roll va Pitch tu du lieu MPU6050 \*/  
void Angle\_Update(const IMU\_Data\_t \*data, float dt\_s);

/\* Lay gia tri goc Roll va Pitch hien tai \*/  
void Angle\_Get(Angle\_Data\_t \*angle);

/\* Thay doi he so loc alpha \*/  
int Angle\_SetAlpha(float alpha);

/\* Lay he so loc alpha hien tai \*/  
float Angle\_GetAlpha(void);

\#endif

\#ifndef FLASH\_CONFIG\_H  
\#define FLASH\_CONFIG\_H

\#include "imu\_types.h"

/\* Địa chỉ lưu cấu hình trên W25Qxx (căn theo sector 4KB) \*/  
\#define FLASH\_CONFIG\_BASE\_ADDR   0x000000UL

/\* Lệnh chuẩn W25Qxx \*/  
\#define W25Q\_CMD\_WRITE\_ENABLE    0x06  
\#define W25Q\_CMD\_READ\_STATUS1    0x05  
\#define W25Q\_CMD\_READ\_DATA       0x03  
\#define W25Q\_CMD\_PAGE\_PROGRAM    0x02  
\#define W25Q\_CMD\_SECTOR\_ERASE    0x20  
\#define W25Q\_CMD\_JEDEC\_ID        0x9F

int Flash\_Init(void);  
int Flash\_SaveConfig(const System\_Config\_t \*config);  
int Flash\_LoadConfig(System\_Config\_t \*config);

\#endif /\* FLASH\_CONFIG\_H \*/

\#include "flash\_config.h"  
\#include "stm32f10x.h"  
\#include \<string.h\>

/\* \===== Cấu hình chân SPI1 cho W25Qxx \===== \*/  
\#define W25Q\_SPI          SPI1  
\#define W25Q\_CS\_PORT      GPIOA  
\#define W25Q\_CS\_PIN       GPIO\_Pin\_4

\#define W25Q\_CS\_LOW()     GPIO\_ResetBits(W25Q\_CS\_PORT, W25Q\_CS\_PIN)  
\#define W25Q\_CS\_HIGH()    GPIO\_SetBits(W25Q\_CS\_PORT, W25Q\_CS\_PIN)

/\* \===== Hàm nội bộ: truyền/nhận 1 byte qua SPI \===== \*/  
static uint8\_t spi\_transfer(uint8\_t tx\_data)  
{  
    while (SPI\_I2S\_GetFlagStatus(W25Q\_SPI, SPI\_I2S\_FLAG\_TXE) \== RESET);  
    SPI\_I2S\_SendData(W25Q\_SPI, tx\_data);  
    while (SPI\_I2S\_GetFlagStatus(W25Q\_SPI, SPI\_I2S\_FLAG\_RXNE) \== RESET);  
    return SPI\_I2S\_ReceiveData(W25Q\_SPI);  
}

/\* \===== Hàm nội bộ: chờ chip rảnh \===== \*/  
static void w25q\_wait\_busy(void)  
{  
    uint8\_t status;  
    W25Q\_CS\_LOW();  
    spi\_transfer(W25Q\_CMD\_READ\_STATUS1);  
    do {  
        status \= spi\_transfer(0xFF);  
    } while (status & 0x01);  
    W25Q\_CS\_HIGH();  
}

static void w25q\_write\_enable(void)  
{  
    W25Q\_CS\_LOW();  
    spi\_transfer(W25Q\_CMD\_WRITE\_ENABLE);  
    W25Q\_CS\_HIGH();  
}

static void w25q\_sector\_erase(uint32\_t addr)  
{  
    w25q\_write\_enable();  
    W25Q\_CS\_LOW();  
    spi\_transfer(W25Q\_CMD\_SECTOR\_ERASE);  
    spi\_transfer((addr \>\> 16\) & 0xFF);  
    spi\_transfer((addr \>\> 8\)  & 0xFF);  
    spi\_transfer(addr & 0xFF);  
    W25Q\_CS\_HIGH();  
    w25q\_wait\_busy();  
}

static void w25q\_page\_program(uint32\_t addr, const uint8\_t \*data, uint16\_t len)  
{  
    w25q\_write\_enable();  
    W25Q\_CS\_LOW();  
    spi\_transfer(W25Q\_CMD\_PAGE\_PROGRAM);  
    spi\_transfer((addr \>\> 16\) & 0xFF);  
    spi\_transfer((addr \>\> 8\)  & 0xFF);  
    spi\_transfer(addr & 0xFF);  
    for (uint16\_t i \= 0; i \< len; i++) {  
        spi\_transfer(data\[i\]);  
    }  
    W25Q\_CS\_HIGH();  
    w25q\_wait\_busy();  
}

static void w25q\_read\_data(uint32\_t addr, uint8\_t \*buf, uint16\_t len)  
{  
    W25Q\_CS\_LOW();  
    spi\_transfer(W25Q\_CMD\_READ\_DATA);  
    spi\_transfer((addr \>\> 16\) & 0xFF);  
    spi\_transfer((addr \>\> 8\)  & 0xFF);  
    spi\_transfer(addr & 0xFF);  
    for (uint16\_t i \= 0; i \< len; i++) {  
        buf\[i\] \= spi\_transfer(0xFF);  
    }  
    W25Q\_CS\_HIGH();  
}

/\* \===== API công khai \===== \*/  
int Flash\_Init(void)  
{  
    GPIO\_InitTypeDef gpio;  
    SPI\_InitTypeDef  spi;  
    uint8\_t id\[3\];

    RCC\_APB2PeriphClockCmd(RCC\_APB2Periph\_GPIOA \| RCC\_APB2Periph\_SPI1, ENABLE);

    /\* PA5 \= SCK, PA7 \= MOSI: AF\_PP \*/  
    gpio.GPIO\_Pin   \= GPIO\_Pin\_5 \| GPIO\_Pin\_7;  
    gpio.GPIO\_Mode  \= GPIO\_Mode\_AF\_PP;  
    gpio.GPIO\_Speed \= GPIO\_Speed\_50MHz;  
    GPIO\_Init(GPIOA, \&gpio);

    /\* PA6 \= MISO: IN\_FLOATING \*/  
    gpio.GPIO\_Pin  \= GPIO\_Pin\_6;  
    gpio.GPIO\_Mode \= GPIO\_Mode\_IN\_FLOATING;  
    GPIO\_Init(GPIOA, \&gpio);

    /\* PA4 \= CS: Out\_PP \*/  
    gpio.GPIO\_Pin   \= W25Q\_CS\_PIN;  
    gpio.GPIO\_Mode  \= GPIO\_Mode\_Out\_PP;  
    gpio.GPIO\_Speed \= GPIO\_Speed\_50MHz;  
    GPIO\_Init(W25Q\_CS\_PORT, \&gpio);  
    W25Q\_CS\_HIGH();

    /\* SPI1 master, mode 0, 8-bit, prescaler 4 (\~18 MHz) \*/  
    spi.SPI\_Direction         \= SPI\_Direction\_2Lines\_FullDuplex;  
    spi.SPI\_Mode              \= SPI\_Mode\_Master;  
    spi.SPI\_DataSize          \= SPI\_DataSize\_8b;  
    spi.SPI\_CPOL              \= SPI\_CPOL\_Low;  
    spi.SPI\_CPHA              \= SPI\_CPHA\_1Edge;  
    spi.SPI\_NSS               \= SPI\_NSS\_Soft;  
    spi.SPI\_BaudRatePrescaler \= SPI\_BaudRatePrescaler\_4;  
    spi.SPI\_FirstBit          \= SPI\_FirstBit\_MSB;  
    spi.SPI\_CRCPolynomial     \= 7;  
    SPI\_Init(W25Q\_SPI, \&spi);  
    SPI\_Cmd(W25Q\_SPI, ENABLE);

    /\* Đọc JEDEC ID: W25Q64 → 0xEF4017 \*/  
    W25Q\_CS\_LOW();  
    spi\_transfer(W25Q\_CMD\_JEDEC\_ID);  
    id\[0\] \= spi\_transfer(0xFF);  
    id\[1\] \= spi\_transfer(0xFF);  
    id\[2\] \= spi\_transfer(0xFF);  
    W25Q\_CS\_HIGH();

    if (id\[0\] \!= 0xEF) {  
        return IMU\_ERR\_FLASH;  
    }  
    return IMU\_OK;  
}

int Flash\_SaveConfig(const System\_Config\_t \*config)  
{  
    if (config \== NULL) return IMU\_ERR\_INVALID\_PARAM;

    System\_Config\_t tmp \= \*config;  
    tmp.magic \= IMU\_CONFIG\_MAGIC;

    /\* Tính checksum XOR (đơn giản, đủ dùng cho đồ án) \*/  
    const uint8\_t \*p \= (const uint8\_t \*)\&tmp;  
    uint32\_t crc \= 0;  
    for (uint32\_t i \= 0; i \< sizeof(System\_Config\_t) \- sizeof(uint32\_t); i++) {  
        crc ^= p\[i\];  
    }  
    tmp.crc32 \= crc;

    w25q\_sector\_erase(FLASH\_CONFIG\_BASE\_ADDR);  
    w25q\_page\_program(FLASH\_CONFIG\_BASE\_ADDR, (const uint8\_t \*)\&tmp, sizeof(System\_Config\_t));

    return IMU\_OK;  
}

int Flash\_LoadConfig(System\_Config\_t \*config)  
{  
    if (config \== NULL) return IMU\_ERR\_INVALID\_PARAM;

    System\_Config\_t tmp;  
    w25q\_read\_data(FLASH\_CONFIG\_BASE\_ADDR, (uint8\_t \*)\&tmp, sizeof(System\_Config\_t));

    if (tmp.magic \!= IMU\_CONFIG\_MAGIC) return IMU\_ERR\_FLASH;

    const uint8\_t \*p \= (const uint8\_t \*)\&tmp;  
    uint32\_t crc \= 0;  
    for (uint32\_t i \= 0; i \< sizeof(System\_Config\_t) \- sizeof(uint32\_t); i++) {  
        crc ^= p\[i\];  
    }  
    if (crc \!= tmp.crc32) return IMU\_ERR\_FLASH;

    \*config \= tmp;  
    return IMU\_OK;  
}

\#ifndef UART\_CMD\_H  
\#define UART\_CMD\_H

\#include "imu\_types.h"

void UART\_Init(void);  
void UART\_Process(void);  
void UART\_SendAngle(const Angle\_Data\_t \*angle);  
void UART\_SendGroupInfo(void);

\#endif /\* UART\_CMD\_H \*/

\#include "uart\_cmd.h"  
\#include "calibration.h"  
\#include "angle\_filter.h"  
\#include "flash\_config.h"  
\#include "stm32f10x.h"  
\#include \<stdio.h\>  
\#include \<string.h\>  
\#include \<stdlib.h\>

/\* \===== Cấu hình USART1 \===== \*/  
\#define UART\_PORT       USART1  
\#define UART\_BAUDRATE   115200  
\#define RX\_BUFFER\_SIZE  128

/\* Biến nội bộ module \*/  
static volatile char     rx\_buffer\[RX\_BUFFER\_SIZE\];  
static volatile uint16\_t rx\_index     \= 0;  
static volatile uint8\_t  has\_new\_cmd  \= 0;  
static char              tx\_buffer\[256\];

/\* \===== Hàm nội bộ: gửi chuỗi qua UART (polling) \===== \*/  
static void uart\_send\_string(const char \*str)  
{  
    while (\*str) {  
        while (USART\_GetFlagStatus(UART\_PORT, USART\_FLAG\_TXE) \== RESET);  
        USART\_SendData(UART\_PORT, (uint16\_t)\*str++);  
    }  
}

/\* \===== Hàm nội bộ: gửi số thực 1 chữ số thập phân \===== \*/  
static void uart\_send\_float(const char \*prefix, float val, const char \*suffix)  
{  
    int32\_t whole \= (int32\_t)val;  
    int32\_t frac  \= (int32\_t)((val \- whole) \* 10.0f);  
    if (frac \< 0\) frac \= \-frac;  
    snprintf(tx\_buffer, sizeof(tx\_buffer), "%s%ld.%ld%s",  
             prefix, (long)whole, (long)frac, suffix);  
    uart\_send\_string(tx\_buffer);  
}

/\* \===== API công khai \===== \*/  
void UART\_Init(void)  
{  
    GPIO\_InitTypeDef gpio;  
    USART\_InitTypeDef usart;  
    NVIC\_InitTypeDef nvic;

    RCC\_APB2PeriphClockCmd(RCC\_APB2Periph\_GPIOA \| RCC\_APB2Periph\_USART1, ENABLE);

    /\* PA9 \= TX: AF\_PP \*/  
    gpio.GPIO\_Pin   \= GPIO\_Pin\_9;  
    gpio.GPIO\_Mode  \= GPIO\_Mode\_AF\_PP;  
    gpio.GPIO\_Speed \= GPIO\_Speed\_50MHz;  
    GPIO\_Init(GPIOA, \&gpio);

    /\* PA10 \= RX: IN\_FLOATING \*/  
    gpio.GPIO\_Pin  \= GPIO\_Pin\_10;  
    gpio.GPIO\_Mode \= GPIO\_Mode\_IN\_FLOATING;  
    GPIO\_Init(GPIOA, \&gpio);

    usart.USART\_BaudRate            \= UART\_BAUDRATE;  
    usart.USART\_WordLength          \= USART\_WordLength\_8b;  
    usart.USART\_StopBits            \= USART\_StopBits\_1;  
    usart.USART\_Parity              \= USART\_Parity\_No;  
    usart.USART\_HardwareFlowControl \= USART\_HardwareFlowControl\_None;  
    usart.USART\_Mode                \= USART\_Mode\_Rx \| USART\_Mode\_Tx;  
    USART\_Init(UART\_PORT, \&usart);

    /\* Bật ngắt RXNE \+ IDLE \*/  
    USART\_ITConfig(UART\_PORT, USART\_IT\_RXNE, ENABLE);  
    USART\_ITConfig(UART\_PORT, USART\_IT\_IDLE, ENABLE);

    nvic.NVIC\_IRQChannel                   \= USART1\_IRQn;  
    nvic.NVIC\_IRQChannelPreemptionPriority \= 1;  
    nvic.NVIC\_IRQChannelSubPriority        \= 1;  
    nvic.NVIC\_IRQChannelCmd                \= ENABLE;  
    NVIC\_Init(\&nvic);

    USART\_Cmd(UART\_PORT, ENABLE);  
}

/\* \===== Trình xử lý lệnh \===== \*/  
static void uart\_process\_command(const char \*cmd)  
{  
    /\* Cắt \\r\\n nếu còn sót \*/  
    char clean\[RX\_BUFFER\_SIZE\];  
    strncpy(clean, cmd, RX\_BUFFER\_SIZE \- 1);  
    clean\[RX\_BUFFER\_SIZE \- 1\] \= '\\0';  
    uint16\_t len \= strlen(clean);  
    while (len \> 0 && (clean\[len-1\] \== '\\r' \|\| clean\[len-1\] \== '\\n')) {  
        clean\[--len\] \= '\\0';  
    }

    /\* 1\. INFO\_REQUEST \*/  
    if (strcmp(clean, "INFO\_REQUEST") \== 0 \|\|  
        strcmp(clean, "INFO\_REQUEST:GROUP") \== 0\) {  
        UART\_SendGroupInfo();  
        return;  
    }

    /\* 2\. CALIBRATE\_IMU \*/  
    if (strcmp(clean, "CALIBRATE\_IMU") \== 0\) {  
        if (Calibration\_Run() \!= IMU\_OK) {  
            uart\_send\_string("ERROR:CALIBRATE\_FAILED\\r\\n");  
            return;  
        }  
        IMU\_Offset\_t off;  
        Calibration\_GetOffset(\&off);

        System\_Config\_t cfg;  
        cfg.offset \= off;  
        cfg.alpha  \= Angle\_GetAlpha();  
        Flash\_SaveConfig(\&cfg);

        uart\_send\_string("CALIBRATE:OK\\r\\n");  
        uart\_send\_float("ACC\_OFFSET:",  off.accel\_offset\_x, ",");  
        uart\_send\_float("",            off.accel\_offset\_y, ",");  
        uart\_send\_float("",            off.accel\_offset\_z, "\\r\\n");  
        uart\_send\_float("GYRO\_OFFSET:", off.gyro\_offset\_x, ",");  
        uart\_send\_float("",            off.gyro\_offset\_y, ",");  
        uart\_send\_float("",            off.gyro\_offset\_z, "\\r\\n");  
        return;  
    }

    /\* 3\. SET\_FILTER\_ALPHA:\<val\> \*/  
    if (strncmp(clean, "SET\_FILTER\_ALPHA:", 17\) \== 0\) {  
        float alpha \= (float)atof(clean \+ 17);  
        if (Angle\_SetAlpha(alpha) \!= IMU\_OK) {  
            uart\_send\_string("ERROR:INVALID\_ALPHA\\r\\n");  
            return;  
        }  
        System\_Config\_t cfg;  
        Calibration\_GetOffset(\&cfg.offset);  
        cfg.alpha \= Angle\_GetAlpha();  
        Flash\_SaveConfig(\&cfg);

        uart\_send\_float("FILTER\_ALPHA:", alpha, "\\r\\n");  
        return;  
    }

    /\* 4\. GET\_ANGLE \*/  
    if (strcmp(clean, "GET\_ANGLE") \== 0\) {  
        Angle\_Data\_t angle;  
        Angle\_Get(\&angle);  
        UART\_SendAngle(\&angle);  
        return;  
    }

    uart\_send\_string("ERROR:UNKNOWN\_CMD\\r\\n");  
}

/\* \===== API công khai: xử lý lệnh trong main loop \===== \*/  
void UART\_Process(void)  
{  
    if (has\_new\_cmd) {  
        has\_new\_cmd \= 0;  
        uart\_process\_command((const char \*)rx\_buffer);  
    }  
}

void UART\_SendAngle(const Angle\_Data\_t \*angle)  
{  
    if (angle \== NULL) return;  
    uart\_send\_float("ROLL:",  angle-\>roll\_deg,  "\\r\\n");  
    uart\_send\_float("PITCH:", angle-\>pitch\_deg, "\\r\\n");  
}

void UART\_SendGroupInfo(void)  
{  
    uart\_send\_string("GROUP\_NAME: Group 04 \- MPU6050 Inclinometer\\r\\n");  
    uart\_send\_string("MEMBERS: Tran Duc Anh, Tran Quoc An, Nguyen Duy Anh, Tran Tuan Anh, Tran Khac Anh\\r\\n");  
    uart\_send\_string("PROJECT\_NAME: Inclinometer Device Using MPU6050\\r\\n");  
    uart\_send\_string("HARDWARE\_USED: STM32, MPU6050, OLED/TFT, W25Qxx\\r\\n");  
    uart\_send\_string("SOFTWARE\_USED: Keil MDK, SPL\\r\\n");  
    uart\_send\_string("DATE: 2026-10-26\\r\\n");  
}

/\* \===== Trình ngắt USART1 \===== \*/  
void USART1\_IRQHandler(void)  
{  
    /\* RXNE: nhận từng byte \*/  
    if (USART\_GetITStatus(UART\_PORT, USART\_IT\_RXNE) \!= RESET) {  
        char ch \= (char)USART\_ReceiveData(UART\_PORT);  
        if (rx\_index \< RX\_BUFFER\_SIZE \- 1\) {  
            rx\_buffer\[rx\_index++\] \= ch;  
        }  
        USART\_ClearITPendingBit(UART\_PORT, USART\_IT\_RXNE);  
    }

    /\* IDLE: kết thúc khung lệnh \*/  
    if (USART\_GetITStatus(UART\_PORT, USART\_IT\_IDLE) \!= RESET) {  
        (void)USART\_ReceiveData(UART\_PORT);   /\* Đọc SR+DR để xóa cờ IDLE \*/  
        rx\_buffer\[rx\_index\] \= '\\0';  
        has\_new\_cmd \= 1;  
        rx\_index \= 0;  
        USART\_ClearITPendingBit(UART\_PORT, USART\_IT\_IDLE);  
    }  
}

\#ifndef IMU\_TYPES\_H  
\#define IMU\_TYPES\_H

\#include \<stdint.h\>  
\#include \<stdbool.h\>

// Hằng số hệ thống //  
\#define IMU\_SAMPLE\_RATE\_HZ       100U     // Trần Khắc Anh: Tần số ngắt Timer lấy mẫu (100 Hz) //  
\#define IMU\_SAMPLE\_PERIOD\_S      0.01f         // Trần Khắc Anh: Chu kỳ lấy mẫu cố định dt \= 10ms //  
\#define IMU\_DEFAULT\_ALPHA        0.98f         // Nguyễn Duy Anh: Hệ số lọc bù mặc định //  
\#define IMU\_ALPHA\_MIN            0.50f         // Trần Tuấn Anh: Ngưỡng alpha nhỏ nhất cho lệnh UART //  
\#define IMU\_ALPHA\_MAX            0.999f        // Trần Tuấn Anh: Ngưỡng alpha lớn nhất cho lệnh UART //  
\#define IMU\_CALIB\_SAMPLES        200U          // Trần Quốc Ân: Số mẫu lấy tĩnh để tính offset //  
\#define IMU\_CONFIG\_MAGIC         0x494D5534UL  // Trần Tuấn Anh: Mã nhận diện dữ liệu Flash hợp lệ ("IMU4") //

// Mã trạng thái trả về chung //  
typedef enum {  
    IMU\_OK                 \=  0, /\* Cả nhóm: Xử lý thành công \*/  
    IMU\_ERR\_I2C            \= \-1, /\* Trần Đức Anh: Lỗi giao tiếp I2C với MPU6050 \*/  
    IMU\_ERR\_DEVICE\_ID      \= \-2, /\* Trần Đức Anh: Sai mã chip WHO\_AM\_I \*/  
    IMU\_ERR\_INVALID\_PARAM  \= \-3, /\* Trần Tuấn Anh: Tham số alpha nhận qua UART không hợp lệ \*/  
    IMU\_ERR\_FLASH          \= \-4, /\* Trần Tuấn Anh: Lỗi giao tiếp SPI hoặc hỏng chip Flash W25Qxx \*/  
    IMU\_ERR\_NOT\_CALIBRATED \= \-5  /\* Trần Quốc Ân: Hệ thống chưa được chạy hiệu chuẩn \*/  
} IMU\_Status\_t;

/\* Dữ liệu cảm biến đã đổi sang đơn vị g và dps \*/  
typedef struct {  
    float ax\_g;     /\* Trần Đức Anh đọc ra, Trần Quốc Ân trừ offset: Gia tốc trục X (g) \*/  
    float ay\_g;     /\* Trần Đức Anh đọc ra, Trần Quốc Ân trừ offset: Gia tốc trục Y (g) \*/  
    float az\_g;     /\* Trần Đức Anh đọc ra, Trần Quốc Ân trừ offset: Gia tốc trục Z (g) \*/  
    float gx\_dps;   /\* Trần Đức Anh đọc ra, Trần Quốc Ân trừ offset: Vận tốc góc trục X (dps) \*/  
    float gy\_dps;   /\* Trần Đức Anh đọc ra, Trần Quốc Ân trừ offset: Vận tốc góc trục Y (dps) \*/  
    float gz\_dps;   /\* Trần Đức Anh đọc ra, Trần Quốc Ân trừ offset: Vận tốc góc trục Z (dps) \*/  
} IMU\_Data\_t;

/\* Kết quả góc nghiêng đầu ra \*/  
typedef struct {  
    float roll\_deg;   /\* Nguyễn Duy Anh tính, Trần Khắc Anh vẽ lên TFT, Trần Tuấn Anh gửi UART: Góc Roll \*/  
    float pitch\_deg;  /\* Nguyễn Duy Anh tính, Trần Khắc Anh vẽ lên TFT, Trần Tuấn Anh gửi UART: Góc Pitch \*/  
} Angle\_Data\_t;

/\* Bộ số sai số tĩnh của cảm biến \*/  
typedef struct {  
    float accel\_offset\_x; /\* Trần Quốc Ân tính ra, Trần Tuấn Anh lưu Flash: Offset gia tốc trục X \*/  
    float accel\_offset\_y; /\* Trần Quốc Ân tính ra, Trần Tuấn Anh lưu Flash: Offset gia tốc trục Y \*/  
    float accel\_offset\_z; /\* Trần Quốc Ân tính ra, Trần Tuấn Anh lưu Flash: Offset gia tốc trục Z (đã trừ 1g) \*/  
    float gyro\_offset\_x;  /\* Trần Quốc Ân tính ra, Trần Tuấn Anh lưu Flash: Offset con quay trục X \*/  
    float gyro\_offset\_y;  /\* Trần Quốc Ân tính ra, Trần Tuấn Anh lưu Flash: Offset con quay trục Y \*/  
    float gyro\_offset\_z;  /\* Trần Quốc Ân tính ra, Trần Tuấn Anh lưu Flash: Offset con quay trục Z \*/  
} IMU\_Offset\_t;

/\* Cấu trúc lưu trữ cấu hình trong Flash W25Qxx \*/  
typedef struct {  
    uint32\_t magic;        /\* Trần Tuấn Anh: Mã kiểm tra 0x494D5534 xác định Flash có dữ liệu \*/  
    IMU\_Offset\_t offset;   /\* Trần Tuấn Anh lưu xuống Flash, đọc lên nạp lại cho Trần Quốc Ân \*/  
    float alpha;           /\* Trần Tuấn Anh lưu xuống Flash, đọc lên nạp lại cho Nguyễn Duy Anh \*/  
    uint32\_t crc32;        /\* Trần Tuấn Anh: Mã kiểm tra toàn vẹn dữ liệu \*/  
} System\_Config\_t;

\#endif /\* IMU\_TYPES\_H \*/

\#ifndef DISPLAY\_SYSTEM\_H  
\#define DISPLAY\_SYSTEM\_H  
\#include "stm32f10x.h"  
typedef enum  
{  
    SYSTEM\_INIT \= 0,          /\* Dang khoi tao \*/  
    SYSTEM\_READY,             /\* Khoi tao thanh cong, san sang \*/  
    SYSTEM\_RUNNING,           /\* Dang doc cam bien va tinh goc \*/  
    SYSTEM\_CALIBRATING,       /\* Dang hieu chuan cam bien \*/  
    SYSTEM\_ERROR              /\* Xay ra loi \*/  
} SystemState;

void DisplaySystem\_Init(void);            /\* Khoi tao man hinh TFT va du lieu he thong.\*/

void DisplaySystem\_TimerInit(void);       /\* Khoi tao TIMER2 de tao chu ky 10 ms \*/

void DisplaySystem\_Process(void);          /\*  Xu ly du lieu MPU6050 va cap nhat goc \*/

void DisplaySystem\_SetState(SystemState state);      /\* Thay doi trang thai hien tai cua he thong \*/

void DisplaySystem\_SetAngle(float roll, float pitch);       /\* Cap nhat gia tri goc Roll va Pitch \*/

\#endif

\#include "stm32f10x.h"  
\#include "display\_system.h"  
\#include \<stdio.h\>  
\#include \<string.h\>

\#include "ili9341.h"              // Dieu khien man hinh TFT  
\#include "mpu6050.h"              // Doc gia toc va van toc goc  
\#include "complementary\_filter.h"       // Ket hop du lieu de tinh goc

/\* Chu ky TIMER va chu ky cap nhat man hinh \*/  
\#define SAMPLE\_TIME\_MS  10U  
\#define LCD\_TIME\_MS     100U

static volatile uint8\_t timer\_flag \= 0;   /\* thoi diem su ly du lieu \*/   
static volatile uint32\_t system\_tick \= 0;   /\* dem so lan TIMER ngat \*/   
static uint32\_t last\_display\_tick \= 0;     /\* Luu moc thoi gian cap nhat man hinh lan truoc \*/  
static float roll \= 0.0f;      /\* goc \*/   
static float pitch \= 0.0f;     /\* goc \*/   
static float alpha \= 0.98f;  
static SystemState state \= SYSTEM\_INIT;    /\* Trang thai ban dau\*/

void DisplaySystem\_SetState(SystemState new\_state)    /\* Ham cho cac module khac thay doi trang thai \*/   
{  
    state \= new\_state;  
}  
void DisplaySystem\_SetAngle(float new\_roll, float new\_pitch)     /\*  Ham cap nhat goc neu module khac can su dung \*/  
{  
    roll \= new\_roll;  
    pitch \= new\_pitch;  
}  
void DisplaySystem\_Init(void)    /\* khoi tao man hinh TFT \*/   
{  
    state \= SYSTEM\_INIT;

    roll \= 0.0f;  
    pitch \= 0.0f;  
    alpha \= 0.98f;  
ILI9341\_Init();  
    ILI9341\_SetRotation(1);  
    ILI9341\_FillScreen(ILI9341\_BLACK);     /\* To mau nen den \*/

    ILI9341\_DrawString(             /\* Hien thi tieu de \*/  
        20, 15,  
        "IMU ANGLE MONITOR",  
        ILI9341\_WHITE,  
        ILI9341\_BLACK,  
        2  
    );  
    ILI9341\_DrawString(              /\* Bao cho nguoi dung biet he thong dang khoi dong \*/  
        20, 55,  
        "Initializing...",  
        ILI9341\_YELLOW,  
        ILI9341\_BLACK,  
        2  
    );  
}  
void DisplaySystem\_TimerInit(void)    /\* cau hinh TIMER2 , ngat sau 10 ms \*/   
{  
    TIM\_TimeBaseInitTypeDef tim;  
    NVIC\_InitTypeDef nvic;

    RCC\_APB1PeriphClockCmd(RCC\_APB1Periph\_TIM2, ENABLE);   /\* bat clock cho TIMER2 , nam tren bus APB1 cua STM32F103 \*/   
    TIM\_TimeBaseStructInit(\&tim);

    tim.TIM\_Prescaler \= 7199;  
    tim.TIM\_Period \= 99;  
    tim.TIM\_CounterMode \= TIM\_CounterMode\_Up;  
    tim.TIM\_ClockDivision \= TIM\_CKD\_DIV1;

    TIM\_TimeBaseInit(TIM2, \&tim);  
    TIM\_ClearITPendingBit(TIM2, TIM\_IT\_Update);  
    TIM\_ITConfig(TIM2, TIM\_IT\_Update, ENABLE);

    nvic.NVIC\_IRQChannel \= TIM2\_IRQn;            /\* cho phep NVIC quan ly ngat TIM2 \*/   
    nvic.NVIC\_IRQChannelPreemptionPriority \= 1;  
    nvic.NVIC\_IRQChannelSubPriority \= 0;  
    nvic.NVIC\_IRQChannelCmd \= ENABLE;

    NVIC\_Init(\&nvic);  
      
    TIM\_Cmd(TIM2, ENABLE);         /\* Bat dau dem TIMER2 \*/  
}  
void TIM2\_IRQHandler(void)     /\* ham ngat TIMER2 \*/   
{  
    if (TIM\_GetITStatus(TIM2, TIM\_IT\_Update) \!= RESET)  
    {  
        TIM\_ClearITPendingBit(TIM2, TIM\_IT\_Update);    /\* Xoa co ngat de TIMER tiep tuc hoat dong \*/  
        system\_tick++;  
        timer\_flag \= 1;  
    }  
}  
static void DisplaySystem\_DrawScreen(void)    /\* giao dien TFT \*/   
{  
    char line\[32\];

     
    ILI9341\_DrawString(      /\* Tieu de man hinh \*/  
        20, 15,  
        "IMU ANGLE MONITOR",  
        ILI9341\_WHITE,  
        ILI9341\_BLACK,  
        2  
    );

      
    ILI9341\_DrawString(        /\* Nhan Roll \*/  
        20, 65,  
        "ROLL:",  
        ILI9341\_CYAN,  
        ILI9341\_BLACK,  
        2  
    );  
    snprintf(line, sizeof(line), "%7.2f deg", (double)roll);

    ILI9341\_DrawString(  
        115, 65,  
        line,  
        ILI9341\_GREEN,  
        ILI9341\_BLACK,  
        2  
    );  
    ILI9341\_DrawString(           /\* Nhan Pitch \*/  
        20, 110,  
        "PITCH:",  
        ILI9341\_CYAN,  
        ILI9341\_BLACK,  
        2  
    );  
    snprintf(line, sizeof(line), "%7.2f deg", (double)pitch);      /\* Chuyen goc Pitch thanh chuoi de hien thi \*/

    ILI9341\_DrawString(  
        115, 110,  
        line,  
        ILI9341\_YELLOW,  
        ILI9341\_BLACK,  
        2  
    );  
    switch (state)  
    {  
        case SYSTEM\_READY:  
            strcpy(line, "READY");  
            break;

        case SYSTEM\_RUNNING:  
            strcpy(line, "RUNNING");  
            break;

        case SYSTEM\_CALIBRATING:  
            strcpy(line, "CALIBRATING");  
            break;

        case SYSTEM\_ERROR:  
            strcpy(line, "SENSOR ERROR");  
            break;

        default:  
            strcpy(line, "INITIALIZING");  
            break;  
    }

    ILI9341\_DrawString(  
        20, 175,  
        "STATUS:",  
        ILI9341\_WHITE,  
        ILI9341\_BLACK,  
        2  
    );

    ILI9341\_DrawString(  
        115, 175,  
        line,  
        (state \== SYSTEM\_ERROR) ?  
        ILI9341\_RED : ILI9341\_WHITE,  
        ILI9341\_BLACK,  
        2  
    );  
}  
void DisplaySystem\_Process(void)   /\* xu ly chinh cua module \*/   
{  
    MPU6050\_Data imu;  
    if (timer\_flag \== 0\)  
    {  
        return;  
    }  
    \_\_disable\_irq();  
    timer\_flag \= 0;  
    \_\_enable\_irq();  
    if (state \== SYSTEM\_ERROR)  
    {  
        return;  
    }  
    if (MPU6050\_ReadData(\&imu) \!= 0\)  
    {  
        state \= SYSTEM\_ERROR;  
        ILI9341\_FillScreen(ILI9341\_BLACK);     /\* Xoa man hinh va thong bao loi cam bien \*/  
        DisplaySystem\_DrawScreen();

        return;  
    }  
    ComplementaryFilter\_Update(     /\* Tinh Roll va Pitch bang Complementary Filter \*/  
        (float)imu.ax,  
        (float)imu.ay,  
        (float)imu.az,  
        (float)imu.gx,  
        (float)imu.gy,  
        (float)imu.gz,  
        0.01f,  
        alpha,  
        \&roll,  
        \&pitch  
    );  
    state \= SYSTEM\_RUNNING;     /\* Cam bien doc duoc du lieu, he thong dang chay \*/  
    if ((system\_tick \- last\_display\_tick) \>=  
        (LCD\_TIME\_MS / SAMPLE\_TIME\_MS))  
    {  
        last\_display\_tick \= system\_tick;  
        DisplaySystem\_DrawScreen();     /\* Ve lai gia tri goc va trang thai \*/  
    }  
}

\#include "stm32f10x.h"  
\#include "display\_system.h"  
\#include "mpu6050.h"  
\#include "complementary\_filter.h"  
\#include "uart.h"  
\#include "flash\_config.h"  
\#include "ili9341.h"

int main(void)  
{  
      
    SystemInit();               /\* khoi tao STM32 \*/   
    UART\_Init(115200);    /\* khoi tao UART \*/  
    DisplaySystem\_Init();   /\* khoi tao TFT \*/  
    if (MPU6050\_Init() \!= 0\)   /\* khoi tao MPU6050 \*/  
    {  
        DisplaySystem\_SetState(SYSTEM\_ERROR);

        while (1)  
        {  
        }  
    }  
    FlashConfig\_Load();                        /\* doc cau hinh Flash \*/  
    ComplementaryFilter\_Init(0.98f);                        /\* khoii tao bo loc \*/  
    DisplaySystem\_SetState(SYSTEM\_READY);  
    DisplaySystem\_TimerInit();                                /\* khoi dong Timer2 \*/  
    while (1)  
    {  
          
        DisplaySystem\_Process();                /\* doc MPU6050, tinh Roll/Pitch \*/  
        UART\_ProcessCommands();            /\* kiem tra lenh UART tu may tinh \*/  
    }  
}  
