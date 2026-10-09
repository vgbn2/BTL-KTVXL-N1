#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>

#define TFT_CS   15
#define TFT_DC   2
#define TFT_RST  4

Adafruit_ILI9341 tft = Adafruit_ILI9341(TFT_CS, TFT_DC, TFT_RST);

void setup() {
  Serial.begin(115200);
  delay(200);

  // Reset pulse
  pinMode(TFT_RST, OUTPUT);
  digitalWrite(TFT_RST, HIGH);
  delay(10);
  digitalWrite(TFT_RST, LOW);
  delay(50);
  digitalWrite(TFT_RST, HIGH);
  delay(150);

  tft.begin(10000000);
  tft.setRotation(1); // Landscape
  tft.fillScreen(ILI9341_NAVY);

  // Header banner
  tft.fillRoundRect(10, 10, 300, 48, 8, ILI9341_BLUE);
  tft.drawRoundRect(10, 10, 300, 48, 8, ILI9341_WHITE);
  tft.setTextColor(ILI9341_WHITE);
  tft.setTextSize(2);
  tft.setCursor(25, 26);
  tft.println("ESP32 + ILI9341 TFT");

  // System status rows
  tft.setTextColor(ILI9341_GREEN);
  tft.setTextSize(2);
  tft.setCursor(20, 75);
  tft.println("Status: ACTIVE");

  tft.setTextColor(ILI9341_YELLOW);
  tft.setCursor(20, 105);
  tft.println("Chip: ESP32-D0WD");

  tft.setTextColor(ILI9341_CYAN);
  tft.setCursor(20, 135);
  tft.println("Res: 320x240 (SPI)");

  tft.setTextColor(ILI9341_WHITE);
  tft.setCursor(20, 165);
  tft.println("Pins: CS:15 DC:2 RS:4");

  // Color spectrum test bar
  uint16_t colors[] = {ILI9341_RED, ILI9341_ORANGE, ILI9341_YELLOW, ILI9341_GREEN, ILI9341_CYAN, ILI9341_BLUE, ILI9341_MAGENTA, ILI9341_WHITE};
  int w = 35;
  for (int i = 0; i < 8; i++) {
    tft.fillRect(20 + i * w, 200, w - 2, 22, colors[i]);
  }

  Serial.println("Dashboard UI initialized successfully.");
}

void loop() {
  static uint32_t counter = 0;
  // Refresh live counter
  tft.fillRect(200, 75, 110, 22, ILI9341_NAVY);
  tft.setCursor(200, 75);
  tft.setTextColor(ILI9341_GREEN);
  tft.setTextSize(2);
  tft.print("#");
  tft.print(counter++);
  delay(1000);
}
