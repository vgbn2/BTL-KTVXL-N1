#include <stdint.h>

#define RCC_BASE        0x40021000u
#define GPIOA_BASE      0x40010800u
#define GPIOB_BASE      0x40010C00u
#define GPIOC_BASE      0x40011000u
#define USART1_BASE     0x40013800u

#define RCC_APB2ENR     (*(volatile uint32_t *)(RCC_BASE + 0x18u))

#define GPIOA_CRL       (*(volatile uint32_t *)(GPIOA_BASE + 0x00u))
#define GPIOA_CRH       (*(volatile uint32_t *)(GPIOA_BASE + 0x04u))

#define GPIOB_CRL       (*(volatile uint32_t *)(GPIOB_BASE + 0x00u))
#define GPIOB_CRH       (*(volatile uint32_t *)(GPIOB_BASE + 0x04u))
#define GPIOB_IDR       (*(volatile uint32_t *)(GPIOB_BASE + 0x08u))
#define GPIOB_ODR       (*(volatile uint32_t *)(GPIOB_BASE + 0x0Cu))
#define GPIOB_BSRR      (*(volatile uint32_t *)(GPIOB_BASE + 0x10u))
#define GPIOB_BRR       (*(volatile uint32_t *)(GPIOB_BASE + 0x14u))

#define GPIOC_CRH       (*(volatile uint32_t *)(GPIOC_BASE + 0x04u))
#define GPIOC_ODR       (*(volatile uint32_t *)(GPIOC_BASE + 0x0Cu))

#define USART1_SR       (*(volatile uint32_t *)(USART1_BASE + 0x00u))
#define USART1_DR       (*(volatile uint32_t *)(USART1_BASE + 0x04u))
#define USART1_BRR      (*(volatile uint32_t *)(USART1_BASE + 0x08u))
#define USART1_CR1      (*(volatile uint32_t *)(USART1_BASE + 0x0Cu))

#define RCC_APB2ENR_AFIOEN   (1u << 0)
#define RCC_APB2ENR_IOPAEN   (1u << 2)
#define RCC_APB2ENR_IOPBEN   (1u << 3)
#define RCC_APB2ENR_IOPCEN   (1u << 4)
#define RCC_APB2ENR_USART1EN (1u << 14)

#define USART_SR_TXE         (1u << 7)
#define USART_CR1_TE         (1u << 3)
#define USART_CR1_UE         (1u << 13)

#define TELEMETRY_FRAME_LEN  15u
#define MPU6050_ADDR         0x68u

// ponytail: memory-mapped telemetry struct for zero-hardware SWD inspection over ST-Link
typedef struct {
    int16_t pitch_x100;
    int16_t roll_x100;
    int16_t ax;
    int16_t ay;
    int16_t az;
    int16_t temp;
    int16_t gx;
    int16_t gy;
    int16_t gz;
    uint16_t is_real_sensor;
    uint32_t sample_count;
} live_telemetry_t;

volatile live_telemetry_t g_telemetry = {0};

typedef struct {
    uint8_t scl_pin;
    uint8_t sda_pin;
    const char *name;
} i2c_bus_t;

void *memset(void *dest, int value, unsigned int len) {
    unsigned char *p = (unsigned char *)dest;
    while (len--) *p++ = (unsigned char)value;
    return dest;
}

static void delay_cycles(volatile uint32_t cycles) {
    while (cycles--) __asm volatile ("nop");
}

static void i2c_delay(void) {
    for (volatile int i = 0; i < 25; i++) __asm volatile ("nop");
}

static uint16_t crc16_ccitt(const uint8_t *data, uint32_t len) {
    uint16_t crc = 0xFFFFu;
    for (uint32_t i = 0; i < len; i++) {
        crc ^= (uint16_t)data[i] << 8;
        for (uint8_t bit = 0; bit < 8; bit++) {
            crc = (crc & 0x8000u) ? (uint16_t)((crc << 1) ^ 0x1021u) : (uint16_t)(crc << 1);
        }
    }
    return crc;
}

static void usart1_write_byte(uint8_t byte) {
    while ((USART1_SR & USART_SR_TXE) == 0) {}
    USART1_DR = byte;
}

static void usart1_write(const uint8_t *data, uint32_t len) {
    for (uint32_t i = 0; i < len; i++) usart1_write_byte(data[i]);
}

static void usart1_write_str(const char *s) {
    while (*s) usart1_write_byte((uint8_t)*s++);
}

static void usart1_write_int(int32_t val) {
    char buf[12];
    int idx = 0;
    if (val == 0) {
        usart1_write_byte('0');
        return;
    }
    if (val < 0) {
        usart1_write_byte('-');
        val = -val;
    }
    while (val > 0) {
        buf[idx++] = (char)('0' + (val % 10));
        val /= 10;
    }
    while (idx > 0) {
        usart1_write_byte((uint8_t)buf[--idx]);
    }
}

/* --- I2C Bit-Bang in Open-Drain Mode --- */
static void i2c_set_scl(const i2c_bus_t *bus, uint8_t val) {
    if (val) GPIOB_BSRR = (1u << bus->scl_pin);
    else     GPIOB_BRR  = (1u << bus->scl_pin);
}

static void i2c_set_sda(const i2c_bus_t *bus, uint8_t val) {
    if (val) GPIOB_BSRR = (1u << bus->sda_pin);
    else     GPIOB_BRR  = (1u << bus->sda_pin);
}

static uint8_t i2c_get_sda(const i2c_bus_t *bus) {
    return (GPIOB_IDR & (1u << bus->sda_pin)) ? 1 : 0;
}

static void i2c_start(const i2c_bus_t *bus) {
    i2c_set_sda(bus, 1);
    i2c_set_scl(bus, 1);
    i2c_delay();
    i2c_set_sda(bus, 0);
    i2c_delay();
    i2c_set_scl(bus, 0);
    i2c_delay();
}

static void i2c_stop(const i2c_bus_t *bus) {
    i2c_set_sda(bus, 0);
    i2c_delay();
    i2c_set_scl(bus, 1);
    i2c_delay();
    i2c_set_sda(bus, 1);
    i2c_delay();
}

static uint8_t i2c_write_byte(const i2c_bus_t *bus, uint8_t data) {
    for (int i = 7; i >= 0; i--) {
        i2c_set_sda(bus, (data >> i) & 1);
        i2c_delay();
        i2c_set_scl(bus, 1);
        i2c_delay();
        i2c_set_scl(bus, 0);
        i2c_delay();
    }
    i2c_set_sda(bus, 1); /* Release SDA for ACK */
    i2c_delay();
    i2c_set_scl(bus, 1);
    i2c_delay();
    uint8_t ack = !i2c_get_sda(bus); /* ACK is active low */
    i2c_set_scl(bus, 0);
    i2c_delay();
    return ack;
}

static uint8_t i2c_read_byte(const i2c_bus_t *bus, uint8_t ack) {
    uint8_t data = 0;
    i2c_set_sda(bus, 1); /* Release SDA */
    for (int i = 7; i >= 0; i--) {
        i2c_delay();
        i2c_set_scl(bus, 1);
        i2c_delay();
        if (i2c_get_sda(bus)) data |= (1 << i);
        i2c_set_scl(bus, 0);
        i2c_delay();
    }
    i2c_set_sda(bus, ack ? 0 : 1);
    i2c_delay();
    i2c_set_scl(bus, 1);
    i2c_delay();
    i2c_set_scl(bus, 0);
    i2c_set_sda(bus, 1);
    i2c_delay();
    return data;
}

/* --- MPU6050 Operations --- */
static uint8_t mpu6050_probe(const i2c_bus_t *bus) {
    i2c_start(bus);
    uint8_t ack = i2c_write_byte(bus, (MPU6050_ADDR << 1));
    i2c_stop(bus);
    return ack;
}

static uint8_t mpu6050_read_reg(const i2c_bus_t *bus, uint8_t reg) {
    i2c_start(bus);
    if (!i2c_write_byte(bus, (MPU6050_ADDR << 1))) { i2c_stop(bus); return 0xFF; }
    i2c_write_byte(bus, reg);
    i2c_start(bus);
    if (!i2c_write_byte(bus, (MPU6050_ADDR << 1) | 1)) { i2c_stop(bus); return 0xFF; }
    uint8_t val = i2c_read_byte(bus, 0);
    i2c_stop(bus);
    return val;
}

static uint8_t mpu6050_write_reg(const i2c_bus_t *bus, uint8_t reg, uint8_t val) {
    i2c_start(bus);
    if (!i2c_write_byte(bus, (MPU6050_ADDR << 1))) { i2c_stop(bus); return 0; }
    i2c_write_byte(bus, reg);
    i2c_write_byte(bus, val);
    i2c_stop(bus);
    return 1;
}

static uint8_t mpu6050_read_raw(const i2c_bus_t *bus,
                                int16_t *ax, int16_t *ay, int16_t *az,
                                int16_t *temp,
                                int16_t *gx, int16_t *gy, int16_t *gz) {
    i2c_start(bus);
    if (!i2c_write_byte(bus, (MPU6050_ADDR << 1))) { i2c_stop(bus); return 0; }
    i2c_write_byte(bus, 0x3B); /* ACCEL_XOUT_H */
    i2c_start(bus);
    if (!i2c_write_byte(bus, (MPU6050_ADDR << 1) | 1)) { i2c_stop(bus); return 0; }

    uint8_t d[14];
    for (int i = 0; i < 13; i++) d[i] = i2c_read_byte(bus, 1);
    d[13] = i2c_read_byte(bus, 0);
    i2c_stop(bus);

    *ax = (int16_t)((d[0] << 8) | d[1]);
    *ay = (int16_t)((d[2] << 8) | d[3]);
    *az = (int16_t)((d[4] << 8) | d[5]);
    *temp = (int16_t)((d[6] << 8) | d[7]);
    *gx = (int16_t)((d[8] << 8) | d[9]);
    *gy = (int16_t)((d[10] << 8) | d[11]);
    *gz = (int16_t)((d[12] << 8) | d[13]);
    return 1;
}

/* Fast self-contained math for -nostdlib tilt angle estimation */
static float fast_sqrt(float x) {
    if (x <= 0.0f) return 0.0f;
    float r = x;
    for (int i = 0; i < 6; i++) r = 0.5f * (r + x / r);
    return r;
}

static float fast_atan2(float y, float x) {
    if (x == 0.0f && y == 0.0f) return 0.0f;
    float abs_y = y < 0 ? -y : y;
    float abs_x = x < 0 ? -x : x;
    float angle;
    if (abs_x >= abs_y) {
        float r = abs_y / abs_x;
        angle = r / (1.0f + 0.28f * r * r);
    } else {
        float r = abs_x / abs_y;
        angle = 1.57079632679f - r / (1.0f + 0.28f * r * r);
    }
    if (x < 0) angle = 3.14159265359f - angle;
    if (y < 0) angle = -angle;
    return angle * (180.0f / 3.14159265359f);
}

static void send_telemetry(uint8_t seq, int16_t pitch_x100, int16_t roll_x100, int16_t yaw_x100, uint16_t flags) {
    uint8_t frame[TELEMETRY_FRAME_LEN] = {0};
    frame[0] = 0xAA;
    frame[1] = 0x55;
    frame[2] = TELEMETRY_FRAME_LEN;
    frame[3] = seq;
    frame[4] = 0x01;
    frame[5] = (uint8_t)(pitch_x100 & 0xFF);
    frame[6] = (uint8_t)((pitch_x100 >> 8) & 0xFF);
    frame[7] = (uint8_t)(roll_x100 & 0xFF);
    frame[8] = (uint8_t)((roll_x100 >> 8) & 0xFF);
    frame[9] = (uint8_t)(yaw_x100 & 0xFF);
    frame[10] = (uint8_t)((yaw_x100 >> 8) & 0xFF);
    frame[11] = (uint8_t)(flags & 0xFF);
    frame[12] = (uint8_t)((flags >> 8) & 0xFF);
    uint16_t crc = crc16_ccitt(frame, 13);
    frame[13] = (uint8_t)(crc & 0xFF);
    frame[14] = (uint8_t)((crc >> 8) & 0xFF);
    usart1_write(frame, TELEMETRY_FRAME_LEN);
}

static void hardware_init(void) {
    RCC_APB2ENR |= RCC_APB2ENR_AFIOEN | RCC_APB2ENR_IOPAEN | RCC_APB2ENR_IOPBEN | RCC_APB2ENR_IOPCEN | RCC_APB2ENR_USART1EN;

    /* PA9 (TX) = AF Push-Pull 50MHz, PA10 (RX) = Input Floating */
    GPIOA_CRH &= ~((0xFu << 4) | (0xFu << 8));
    GPIOA_CRH |=  (0xBu << 4) | (0x4u << 8);

    /* PC13 (Onboard LED) = Output Push-Pull 2MHz */
    GPIOC_CRH &= ~(0xFu << 20);
    GPIOC_CRH |=  (0x2u << 20);
    GPIOC_ODR |=  (1u << 13); /* LED OFF */

    /* Configure I2C Pins as Open-Drain Output 50MHz (0x7) */
    /* Right Row: PB10 (SCL) & PB11 (SDA) -> bits 8..15 in CRH */
    GPIOB_CRH &= ~0x0000FF00u;
    GPIOB_CRH |=  0x00007700u;
    GPIOB_BSRR = (1u << 10) | (1u << 11);

    /* Left Row: PB6 (SCL) & PB7 (SDA) -> bits 24..31 in CRL */
    GPIOB_CRL &= ~0xFF000000u;
    GPIOB_CRL |=  0x77000000u;
    GPIOB_BSRR = (1u << 6) | (1u << 7);

    /* USART1 115200 Baud @ 8MHz HSI */
    USART1_BRR = 0x0045u;
    USART1_CR1 = USART_CR1_TE | USART_CR1_UE;
}

int main(void) {
    hardware_init();
    delay_cycles(400000);

    usart1_write_str("\r\n=========================================\r\n");
    usart1_write_str(" STM32F103 MPU6050 DUAL-ROW BRINGUP V2.0 \r\n");
    usart1_write_str("=========================================\r\n");

    const i2c_bus_t bus_right = { .scl_pin = 10, .sda_pin = 11, .name = "Right Row (PB10/PB11)" };
    const i2c_bus_t bus_left  = { .scl_pin = 6,  .sda_pin = 7,  .name = "Left Row (PB6/PB7)" };
    const i2c_bus_t *active_bus = 0;

    /* Scan buses */
    usart1_write_str("[I2C] Scanning Right Row (PB10=SCL, PB11=SDA)...\r\n");
    if (mpu6050_probe(&bus_right)) {
        active_bus = &bus_right;
    } else {
        usart1_write_str("[I2C] Scanning Left Row (PB6=SCL, PB7=SDA)...\r\n");
        if (mpu6050_probe(&bus_left)) {
            active_bus = &bus_left;
        }
    }

    if (active_bus) {
        usart1_write_str("[I2C_OK] Sensor ACK on ");
        usart1_write_str(active_bus->name);
        usart1_write_str("!\r\n");

        /* Wake up sensor (PWR_MGMT_1 = 0) */
        mpu6050_write_reg(active_bus, 0x6B, 0x00);
        delay_cycles(100000);

        uint8_t who = mpu6050_read_reg(active_bus, 0x75);
        usart1_write_str("[I2C_OK] WHO_AM_I = 0x");
        char hex[3];
        const char *digits = "0123456789ABCDEF";
        hex[0] = digits[(who >> 4) & 0xF];
        hex[1] = digits[who & 0xF];
        hex[2] = '\0';
        usart1_write_str(hex);
        usart1_write_str("\r\n");

        /* Gyro config +/-250 dps, Accel config +/-2g */
        mpu6050_write_reg(active_bus, 0x1B, 0x00);
        mpu6050_write_reg(active_bus, 0x1C, 0x00);
    } else {
        usart1_write_str("[I2C_WARN] No sensor detected at 0x68 on either row.\r\n");
        usart1_write_str("[I2C_WARN] Ensure: GND, 3V3, SCL=B10, SDA=B11, AD0=GND.\r\n");
    }

    uint8_t seq = 0;
    int16_t sim_angle = -3000;
    int16_t sim_step = 100;

    while (1) {
        GPIOC_ODR ^= (1u << 13); /* Toggle LED */

        /* Re-probe if sensor wasn't found initially */
        if (!active_bus) {
            if (mpu6050_probe(&bus_right)) {
                active_bus = &bus_right;
                mpu6050_write_reg(active_bus, 0x6B, 0x00);
            } else if (mpu6050_probe(&bus_left)) {
                active_bus = &bus_left;
                mpu6050_write_reg(active_bus, 0x6B, 0x00);
            }
        }

        if (active_bus) {
            int16_t ax = 0, ay = 0, az = 0, temp = 0, gx = 0, gy = 0, gz = 0;
            if (mpu6050_read_raw(active_bus, &ax, &ay, &az, &temp, &gx, &gy, &gz)) {
                float fax = (float)ax / 16384.0f;
                float fay = (float)ay / 16384.0f;
                float faz = (float)az / 16384.0f;

                float pitch = fast_atan2(fay, fast_sqrt(fax * fax + faz * faz));
                float roll  = fast_atan2(-fax, faz);

                int16_t pitch_x100 = (int16_t)(pitch * 100.0f);
                int16_t roll_x100  = (int16_t)(roll * 100.0f);

                g_telemetry.pitch_x100     = pitch_x100;
                g_telemetry.roll_x100      = roll_x100;
                g_telemetry.ax             = ax;
                g_telemetry.ay             = ay;
                g_telemetry.az             = az;
                g_telemetry.temp           = temp;
                g_telemetry.gx             = gx;
                g_telemetry.gy             = gy;
                g_telemetry.gz             = gz;
                g_telemetry.is_real_sensor = 1;
                g_telemetry.sample_count++;

                send_telemetry(seq++, pitch_x100, roll_x100, 0, 0x0003);

                usart1_write_str("[LIVE] P:");
                usart1_write_int(pitch_x100);
                usart1_write_str(" R:");
                usart1_write_int(roll_x100);
                usart1_write_str(" AX:");
                usart1_write_int(ax);
                usart1_write_str(" AY:");
                usart1_write_int(ay);
                usart1_write_str(" AZ:");
                usart1_write_int(az);
                usart1_write_str("\r\n");
            } else {
                active_bus = 0; /* Lost connection */
            }
        } else {
            /* Simulated fallback */
            g_telemetry.pitch_x100     = sim_angle;
            g_telemetry.roll_x100      = (int16_t)(-sim_angle / 2);
            g_telemetry.ax             = 0;
            g_telemetry.ay             = 0;
            g_telemetry.az             = 16384;
            g_telemetry.temp           = 2500;
            g_telemetry.gx             = 0;
            g_telemetry.gy             = 0;
            g_telemetry.gz             = 0;
            g_telemetry.is_real_sensor = 0;
            g_telemetry.sample_count++;

            send_telemetry(seq++, sim_angle, (int16_t)(-sim_angle / 2), 0, 0x0001);
            sim_angle += sim_step;
            if (sim_angle > 3000 || sim_angle < -3000) sim_step = (int16_t)-sim_step;
        }

        delay_cycles(400000);
    }
}
