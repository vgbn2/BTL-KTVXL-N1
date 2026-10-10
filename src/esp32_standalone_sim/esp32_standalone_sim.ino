#include <Wire.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>
#include <math.h>

#define TFT_CS   15
#define TFT_DC   2
#define TFT_RST  4

#define MPU_SDA  21
#define MPU_SCL  22
#define MPU_ADDR 0x68

Adafruit_ILI9341 tft = Adafruit_ILI9341(TFT_CS, TFT_DC, TFT_RST);

/* Complementary Filter State */
float alpha = 0.98f;
float dt = 0.02f; /* 50 Hz loop */
float pitch = 0.0f;
float roll = 0.0f;
float yaw = 0.0f;

static void draw_dashboard_ui(void) {
    tft.fillScreen(ILI9341_BLACK);

    /* Top Title Header */
    tft.fillRoundRect(8, 8, 304, 38, 6, ILI9341_NAVY);
    tft.drawRoundRect(8, 8, 304, 38, 6, ILI9341_CYAN);
    tft.setTextColor(ILI9341_WHITE);
    tft.setTextSize(2);
    tft.setCursor(20, 18);
    tft.print("WOKWI 3D TILT SIM");

    tft.setTextSize(1);
    tft.setTextColor(ILI9341_GREEN);
    tft.setCursor(240, 22);
    tft.print("ONLINE");

    /* Horizon Box (Left side: 140x140) */
    tft.drawRect(12, 54, 140, 140, ILI9341_DARKGREY);
    tft.drawFastHLine(14, 124, 136, ILI9341_DARKCYAN);
    tft.drawFastVLine(82, 56, 136, ILI9341_DARKCYAN);

    /* Readouts Panel (Right side) */
    tft.drawRoundRect(160, 54, 150, 140, 6, ILI9341_BLUE);

    tft.setTextSize(1);
    tft.setTextColor(ILI9341_WHITE);
    tft.setCursor(170, 66);
    tft.print("PITCH (DEG):");

    tft.setCursor(170, 108);
    tft.print("ROLL (DEG):");

    tft.setCursor(170, 150);
    tft.print("FILTER ALPHA:");

    /* Bottom Info Bar */
    tft.fillRect(0, 204, 320, 36, ILI9341_DARKGREY);
    tft.setTextColor(ILI9341_WHITE);
    tft.setTextSize(1);
    tft.setCursor(10, 216);
    tft.print("DRAG MPU6050 3D CUBE TO SIMULATE TILT");
}

static void update_readouts(float p, float r) {
    /* Pitch Digital Readout */
    tft.fillRect(170, 78, 130, 22, ILI9341_BLACK);
    tft.setCursor(170, 80);
    tft.setTextSize(2);
    tft.setTextColor(fabs(p) > 20.0f ? ILI9341_RED : ILI9341_GREEN);
    tft.printf("%+06.2f", p);

    /* Roll Digital Readout */
    tft.fillRect(170, 120, 130, 22, ILI9341_BLACK);
    tft.setCursor(170, 122);
    tft.setTextSize(2);
    tft.setTextColor(fabs(r) > 20.0f ? ILI9341_RED : ILI9341_GREEN);
    tft.printf("%+06.2f", r);

    /* Filter Alpha Readout */
    tft.fillRect(170, 162, 130, 20, ILI9341_BLACK);
    tft.setCursor(170, 164);
    tft.setTextSize(2);
    tft.setTextColor(ILI9341_CYAN);
    tft.printf("%.4f", alpha);

    /* Horizon Crosshair Representation */
    static int last_cx = 82;
    static int last_cy = 124;
    tft.fillCircle(last_cx, last_cy, 6, ILI9341_BLACK);
    tft.drawFastHLine(14, 124, 136, ILI9341_DARKCYAN);
    tft.drawFastVLine(82, 56, 136, ILI9341_DARKCYAN);

    int cx = 82 + (int)(r * 1.2f);
    int cy = 124 - (int)(p * 1.2f);
    if (cx < 20) cx = 20; if (cx > 144) cx = 144;
    if (cy < 62) cy = 62; if (cy > 186) cy = 186;

    tft.fillCircle(cx, cy, 6, ILI9341_YELLOW);
    tft.drawCircle(cx, cy, 6, ILI9341_WHITE);
    last_cx = cx;
    last_cy = cy;
}

void setup() {
    Serial.begin(115200);
    Wire.begin(MPU_SDA, MPU_SCL);

    /* Wake up MPU6050 */
    Wire.beginTransmission(MPU_ADDR);
    Wire.write(0x6B); // PWR_MGMT_1
    Wire.write(0x00); // Wake up
    Wire.endTransmission(true);

    /* Initialize Display */
    pinMode(TFT_RST, OUTPUT);
    digitalWrite(TFT_RST, HIGH); delay(10);
    digitalWrite(TFT_RST, LOW);  delay(50);
    digitalWrite(TFT_RST, HIGH); delay(150);

    tft.begin(27000000);
    tft.setRotation(1);
    draw_dashboard_ui();

    Serial.println("Wokwi Tilt Simulator Ready. Drag MPU6050 in UI!");
}

void loop() {
    /* Read 14 bytes from MPU6050 */
    Wire.beginTransmission(MPU_ADDR);
    Wire.write(0x3B);
    Wire.endTransmission(false);
    Wire.requestFrom(MPU_ADDR, 14, true);

    if (Wire.available() >= 14) {
        int16_t raw_ax = (Wire.read() << 8) | Wire.read();
        int16_t raw_ay = (Wire.read() << 8) | Wire.read();
        int16_t raw_az = (Wire.read() << 8) | Wire.read();
        int16_t raw_temp = (Wire.read() << 8) | Wire.read();
        (void)raw_temp;
        int16_t raw_gx = (Wire.read() << 8) | Wire.read();
        int16_t raw_gy = (Wire.read() << 8) | Wire.read();
        int16_t raw_gz = (Wire.read() << 8) | Wire.read();

        float fax = (float)raw_ax / 16384.0f;
        float fay = (float)raw_ay / 16384.0f;
        float faz = (float)raw_az / 16384.0f;

        float rate_pitch = (float)raw_gx / 131.0f;
        float rate_roll  = (float)raw_gy / 131.0f;

        float pitch_accel = atan2f(fay, sqrtf(fax * fax + faz * faz)) * 57.2957795f;
        float roll_accel  = atan2f(-fax, faz) * 57.2957795f;

        /* Complementary filter update */
        pitch = alpha * (pitch + rate_pitch * dt) + (1.0f - alpha) * pitch_accel;
        roll  = alpha * (roll  + rate_roll  * dt) + (1.0f - alpha) * roll_accel;

        update_readouts(pitch, roll);
    }

    delay(20); /* 50 Hz refresh */
}
