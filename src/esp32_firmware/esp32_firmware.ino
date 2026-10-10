#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>

/* Pin Definitions matching physical wiring and Wiring_and_Pinouts.md */
#define TFT_CS   15
#define TFT_DC   2
#define TFT_RST  4

#define STM32_RX_PIN 16  /* ESP32 RX2 connected to STM32 TX (PA9) */
#define STM32_TX_PIN 17  /* ESP32 TX2 connected to STM32 RX (PA10) */

Adafruit_ILI9341 tft = Adafruit_ILI9341(TFT_CS, TFT_DC, TFT_RST);

/* Binary Telemetry Protocol Constants */
#define TELEMETRY_MAGIC_0 0xAA
#define TELEMETRY_MAGIC_1 0x55
#define TELEMETRY_FRAME_LEN 15

#pragma pack(push, 1)
typedef struct {
    uint8_t magic0;
    uint8_t magic1;
    uint8_t len;
    uint8_t seq;
    uint8_t type;
    int16_t pitch_x100;
    int16_t roll_x100;
    int16_t yaw_x100;
    uint16_t status_flag;
    uint16_t crc16;
} telemetry_frame_t;
#pragma pack(pop)

/* State Variables */
static float current_pitch = 0.0f;
static float current_roll = 0.0f;
static float current_yaw = 0.0f;
static uint16_t current_status = 0;
static uint32_t packet_count = 0;
static uint32_t crc_err_count = 0;
static uint32_t last_fps_time = 0;
static uint32_t fps_counter = 0;
static uint32_t displayed_fps = 0;

static uint8_t rx_buf[32];
static uint8_t rx_idx = 0;

/* CRC16-CCITT implementation */
static uint16_t calculate_crc16(const uint8_t *data, size_t len) {
    uint16_t crc = 0xFFFF;
    for (size_t i = 0; i < len; i++) {
        crc ^= ((uint16_t)data[i] << 8);
        for (uint8_t bit = 0; bit < 8; bit++) {
            if (crc & 0x8000) {
                crc = (crc << 1) ^ 0x1021;
            } else {
                crc = crc << 1;
            }
        }
    }
    return crc;
}

static void draw_dashboard_ui(void) {
    tft.fillScreen(ILI9341_BLACK);

    /* Top Title Header */
    tft.fillRoundRect(8, 8, 304, 38, 6, ILI9341_NAVY);
    tft.drawRoundRect(8, 8, 304, 38, 6, ILI9341_CYAN);
    tft.setTextColor(ILI9341_WHITE);
    tft.setTextSize(2);
    tft.setCursor(20, 18);
    tft.print("TILT MONITOR N1");

    /* Subtitle & status */
    tft.setTextSize(1);
    tft.setTextColor(ILI9341_GREEN);
    tft.setCursor(220, 22);
    tft.print("STM32 LINK");

    /* Horizon Box (Left side: 140x140) */
    tft.drawRect(12, 54, 140, 140, ILI9341_DARKGREY);
    tft.drawFastHLine(14, 124, 136, ILI9341_DARKCYAN);
    tft.drawFastVLine(82, 56, 136, ILI9341_DARKCYAN);

    /* Readouts Panel (Right side) */
    tft.drawRoundRect(160, 54, 150, 140, 6, ILI9341_BLUE);

    tft.setTextSize(1);
    tft.setTextColor(ILI9341_WHITE);
    tft.setCursor(170, 66);
    tft.print("PITCH ANGLE:");

    tft.setCursor(170, 108);
    tft.print("ROLL ANGLE:");

    tft.setCursor(170, 150);
    tft.print("YAW (HEADING):");

    /* Bottom Info Bar */
    tft.fillRect(0, 204, 320, 36, ILI9341_DARKGREY);
    tft.setTextColor(ILI9341_WHITE);
    tft.setTextSize(1);
    tft.setCursor(10, 216);
    tft.print("PKTS: 0  CRC_ERR: 0  RATE: 0 FPS");
}

static void update_readouts(float pitch, float roll, float yaw) {
    /* Pitch Digital Readout */
    tft.fillRect(170, 78, 130, 22, ILI9341_BLACK);
    tft.setCursor(170, 80);
    tft.setTextSize(2);
    if (fabs(pitch) > 30.0f) {
        tft.setTextColor(ILI9341_RED);
    } else if (fabs(pitch) > 15.0f) {
        tft.setTextColor(ILI9341_YELLOW);
    } else {
        tft.setTextColor(ILI9341_GREEN);
    }
    tft.printf("%+06.2f deg", pitch);

    /* Roll Digital Readout */
    tft.fillRect(170, 120, 130, 22, ILI9341_BLACK);
    tft.setCursor(170, 122);
    tft.setTextSize(2);
    if (fabs(roll) > 30.0f) {
        tft.setTextColor(ILI9341_RED);
    } else if (fabs(roll) > 15.0f) {
        tft.setTextColor(ILI9341_YELLOW);
    } else {
        tft.setTextColor(ILI9341_GREEN);
    }
    tft.printf("%+06.2f deg", roll);

    /* Yaw Digital Readout */
    tft.fillRect(170, 162, 130, 20, ILI9341_BLACK);
    tft.setCursor(170, 164);
    tft.setTextSize(2);
    tft.setTextColor(ILI9341_CYAN);
    tft.printf("%+06.2f deg", yaw);

    /* Horizon Crosshair Representation */
    static int last_cx = 82;
    static int last_cy = 124;
    tft.fillCircle(last_cx, last_cy, 6, ILI9341_BLACK);
    tft.drawFastHLine(14, 124, 136, ILI9341_DARKCYAN);
    tft.drawFastVLine(82, 56, 136, ILI9341_DARKCYAN);

    int cx = 82 + (int)(roll * 1.2f);
    int cy = 124 - (int)(pitch * 1.2f);
    if (cx < 20) cx = 20; if (cx > 144) cx = 144;
    if (cy < 62) cy = 62; if (cy > 186) cy = 186;

    tft.fillCircle(cx, cy, 6, ILI9341_YELLOW);
    tft.drawCircle(cx, cy, 6, ILI9341_WHITE);
    last_cx = cx;
    last_cy = cy;
}

void setup() {
    /* 1. USB Serial to PC (Debug Console) */
    Serial.begin(115200);
    delay(100);
    Serial.println("\r\n=== ESP32 ILI9341 TILT MONITOR INITIALIZING ===");

    /* 2. Hardware UART2 to STM32 */
    Serial2.begin(115200, SERIAL_8N1, STM32_RX_PIN, STM32_TX_PIN);
    Serial.printf("[UART] Serial2 initialized on RX=%d, TX=%d @ 115200\r\n", STM32_RX_PIN, STM32_TX_PIN);

    /* 3. Initialize Display with Reset Pulse */
    pinMode(TFT_RST, OUTPUT);
    digitalWrite(TFT_RST, HIGH); delay(10);
    digitalWrite(TFT_RST, LOW);  delay(50);
    digitalWrite(TFT_RST, HIGH); delay(150);

    tft.begin(27000000); /* 27 MHz SPI */
    tft.setRotation(1);  /* Landscape 320x240 */
    draw_dashboard_ui();

    Serial.println("[TFT] ILI9341 Dashboard Ready.");
}

void loop() {
    /* 1. Bridge PC Serial -> STM32 */
    while (Serial.available()) {
        char c = (char)Serial.read();
        Serial2.write(c);
    }

    /* 2. Parse Incoming Telemetry / ASCII from STM32 */
    while (Serial2.available()) {
        uint8_t byte = Serial2.read();

        /* If we are at the start of a packet, look for magic 0xAA 0x55 */
        if (rx_idx == 0) {
            if (byte == TELEMETRY_MAGIC_0) {
                rx_buf[rx_idx++] = byte;
            } else {
                /* Echo ASCII response lines from STM32 back to PC Serial */
                Serial.write(byte);
            }
        } else if (rx_idx == 1) {
            if (byte == TELEMETRY_MAGIC_1) {
                rx_buf[rx_idx++] = byte;
            } else {
                rx_idx = 0;
            }
        } else {
            rx_buf[rx_idx++] = byte;

            if (rx_idx == TELEMETRY_FRAME_LEN) {
                /* Full packet received, verify CRC16 */
                uint16_t expected_crc = calculate_crc16(rx_buf, TELEMETRY_FRAME_LEN - 2);
                telemetry_frame_t *frame = (telemetry_frame_t *)rx_buf;

                if (frame->crc16 == expected_crc) {
                    current_pitch = (float)frame->pitch_x100 / 100.0f;
                    current_roll  = (float)frame->roll_x100 / 100.0f;
                    current_yaw   = (float)frame->yaw_x100 / 100.0f;
                    current_status = frame->status_flag;
                    packet_count++;
                    fps_counter++;

                    /* Render UI Updates */
                    update_readouts(current_pitch, current_roll, current_yaw);
                } else {
                    crc_err_count++;
                }

                rx_idx = 0; /* Reset state */
            }
        }
    }

    /* 3. Update FPS and Packet Statistics every 1 second */
    uint32_t now = millis();
    if (now - last_fps_time >= 1000) {
        displayed_fps = fps_counter;
        fps_counter = 0;
        last_fps_time = now;

        tft.fillRect(10, 214, 300, 16, ILI9341_DARKGREY);
        tft.setTextColor(ILI9341_WHITE);
        tft.setTextSize(1);
        tft.setCursor(10, 218);
        tft.printf("PKTS:%u  CRC_ERR:%u  RATE:%u FPS", packet_count, crc_err_count, displayed_fps);
    }
}
