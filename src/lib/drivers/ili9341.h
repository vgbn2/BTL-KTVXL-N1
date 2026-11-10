#ifndef ILI9341_H
#define ILI9341_H
#include <stdint.h>
#define ILI9341_BLACK 0x0000
#define ILI9341_WHITE 0xFFFF
#define ILI9341_GREEN 0x07E0
#define ILI9341_RED 0xF800
int ILI9341_Init(void);
int ILI9341_FillScreen(uint16_t color);
int ILI9341_DrawChar(uint16_t x, uint16_t y, char ch, uint16_t color);
#endif
