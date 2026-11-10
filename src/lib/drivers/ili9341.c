#include "ili9341.h"
#include "system_time.h"

#if defined(USE_STDPERIPH_DRIVER) || defined(STM32F10X)

// ILI9341 SPI2, man hinh ngang 320 x 240, RGB565.
static int lcd_error = 0;

static void select_lcd(int selected)
{
    if (selected) GPIO_ResetBits(GPIOB, GPIO_Pin_12);
    else GPIO_SetBits(GPIOB, GPIO_Pin_12);
}

static void send_byte(uint8_t value)
{
    uint32_t start = SystemTime_GetMillis();
    if (lcd_error) return;
    while (SPI_I2S_GetFlagStatus(SPI2, SPI_I2S_FLAG_TXE) == RESET)
    {
        if ((uint32_t)(SystemTime_GetMillis() - start) >= 10U)
        {
            lcd_error = 1;
            return;
        }
    }
    SPI_I2S_SendData(SPI2, value);
    while (SPI_I2S_GetFlagStatus(SPI2, SPI_I2S_FLAG_RXNE) == RESET)
    {
        if ((uint32_t)(SystemTime_GetMillis() - start) >= 10U)
        {
            lcd_error = 1;
            return;
        }
    }
    (void)SPI_I2S_ReceiveData(SPI2);
}

static void command(uint8_t value)
{
    GPIO_ResetBits(GPIOB, GPIO_Pin_10);
    send_byte(value);
    GPIO_SetBits(GPIOB, GPIO_Pin_10);
}

static void word(uint16_t value)
{
    send_byte((uint8_t)(value >> 8));
    send_byte((uint8_t)(value & 0xFF));
}

static void window(uint16_t x, uint16_t y, uint16_t width, uint16_t height)
{
    command(0x2A);
    word(x);
    word((uint16_t)(x + width - 1U));
    command(0x2B);
    word(y);
    word((uint16_t)(y + height - 1U));
    command(0x2C);
}

int ILI9341_Init(void)
{
    GPIO_InitTypeDef gpio;
    SPI_InitTypeDef spi;
    lcd_error = 0;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_SPI2, ENABLE);

    GPIO_SetBits(GPIOB, GPIO_Pin_10 | GPIO_Pin_11 | GPIO_Pin_12);
    gpio.GPIO_Pin   = GPIO_Pin_10 | GPIO_Pin_11 | GPIO_Pin_12;
    gpio.GPIO_Mode  = GPIO_Mode_Out_PP;
    gpio.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOB, &gpio);

    gpio.GPIO_Pin   = GPIO_Pin_13 | GPIO_Pin_15;
    gpio.GPIO_Mode  = GPIO_Mode_AF_PP;
    GPIO_Init(GPIOB, &gpio);

    gpio.GPIO_Pin  = GPIO_Pin_14;
    gpio.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(GPIOB, &gpio);

    SPI_StructInit(&spi);
    spi.SPI_Mode              = SPI_Mode_Master;
    spi.SPI_NSS               = SPI_NSS_Soft;
    spi.SPI_BaudRatePrescaler = SPI_BaudRatePrescaler_4;
    SPI_Init(SPI2, &spi);
    SPI_Cmd(SPI2, ENABLE);

    GPIO_ResetBits(GPIOB, GPIO_Pin_11);
    Delay_ms(20);
    GPIO_SetBits(GPIOB, GPIO_Pin_11);
    Delay_ms(120);

    select_lcd(1);
    command(0x01);
    select_lcd(0);
    Delay_ms(150);

    select_lcd(1);
    command(0xC0); send_byte(0x23);
    command(0xC1); send_byte(0x10);
    command(0xC5); send_byte(0x3E); send_byte(0x28);
    command(0xC7); send_byte(0x86);
    command(0x36); send_byte(0x28);
    command(0x3A); send_byte(0x55);
    command(0xB1); send_byte(0x00); send_byte(0x18);
    command(0xB6); send_byte(0x08); send_byte(0x82); send_byte(0x27);
    command(0x11);
    select_lcd(0);
    Delay_ms(120);

    select_lcd(1);
    command(0x29);
    select_lcd(0);
    Delay_ms(20);

    return lcd_error ? -1 : 0;
}

int ILI9341_FillScreen(uint16_t color)
{
    uint32_t i;
    select_lcd(1);
    window(0, 0, 320, 240);
    for (i = 0; i < 320UL * 240UL && !lcd_error; i++)
    {
        word(color);
    }
    select_lcd(0);
    return lcd_error ? -1 : 0;
}

// Font 5x7 cho chu in hoa, so va ky hieu can dung.
static const char glyph_names[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ-.: ";
static const uint8_t glyphs[][5] = {
    {62,81,73,69,62},{0,66,127,64,0},{66,97,81,73,70},{33,65,69,75,49},
    {24,20,18,127,16},{39,69,69,69,57},{60,74,73,73,48},{1,113,9,5,3},
    {54,73,73,73,54},{6,73,73,41,30},
    {126,17,17,17,126},{127,73,73,73,54},{62,65,65,65,34},{127,65,65,34,28},
    {127,73,73,73,65},{127,9,9,9,1},{62,65,73,73,122},{127,8,8,8,127},
    {0,65,127,65,0},{32,64,65,63,1},{127,8,20,34,65},{127,64,64,64,64},
    {127,2,12,2,127},{127,4,8,16,127},{62,65,65,65,62},{127,9,9,9,6},
    {62,65,81,33,94},{127,9,25,41,70},{70,73,73,73,49},{1,1,127,1,1},
    {63,64,64,64,63},{31,32,64,32,31},{63,64,56,64,63},{99,20,8,20,99},
    {7,8,112,8,7},{97,81,73,69,67},
    {8,8,8,8,8},{0,96,96,0,0},{0,54,54,0,0},{0,0,0,0,0}
};

int ILI9341_DrawChar(uint16_t x, uint16_t y, char ch, uint16_t color)
{
    uint32_t glyph = 39U;
    uint32_t i, row, col;
    if (x + 12U > 320U || y + 16U > 240U) return -1;
    for (i = 0; glyph_names[i]; i++)
    {
        if (glyph_names[i] == ch)
        {
            glyph = i;
            break;
        }
    }
    select_lcd(1);
    window(x, y, 12, 16);
    for (row = 0; row < 16U && !lcd_error; row++)
    {
        for (col = 0; col < 12U && !lcd_error; col++)
        {
            word((col < 10U && row < 14U && (glyphs[glyph][col / 2U] & (1U << (row / 2U))))
                 ? color : ILI9341_BLACK);
        }
    }
    select_lcd(0);
    return lcd_error ? -1 : 0;
}

#else

/* Host simulation stubs */
int ILI9341_Init(void) { return 0; }
int ILI9341_FillScreen(uint16_t color) { (void)color; return 0; }
int ILI9341_DrawChar(uint16_t x, uint16_t y, char ch, uint16_t color)
{
    (void)x; (void)y; (void)ch; (void)color;
    return 0;
}

#endif
