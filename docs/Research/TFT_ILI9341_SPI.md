# ILI9341 320x240 SPI TFT Display Controller Reference

**Document Version:** 1.0  
**Target Hardware:** ILI9341 2.8" SPI TFT LCD (240x320 pixels) driven by ESP32-WROOM-32

---

## 1. Controller Architecture & Specifications

The ILI9341 is a single-chip a-Si TFT LCD controller/driver designed for QVGA ($240 \times 320$) resolution displays with an integrated graphic display data RAM (GRAM) of $172,800$ bytes ($240 \times 320 \times 18 \text{ bits} / 8$).

| Parameter | Specification | Project Application |
|---|---|---|
| **Active Resolution** | $240 \times 320$ pixels (Portrait) or $320 \times 240$ (Landscape) | Landscape mode ($320 \times 240$) for dashboard telemetry |
| **Color Depth** | 262K colors (18-bit) or 65K colors (16-bit RGB565) | 16-bit RGB565 (2 bytes per pixel) |
| **Interface** | 4-Wire Serial Peripheral Interface (SPI Mode 0) | Hardware VSPI on ESP32 |
| **Logic Operating Voltage** | $1.65\text{V} - 3.3\text{V}$ (VDDI) | 3.3V logic (natively compatible with ESP32 GPIOs) |
| **Module Supply Voltage** | $3.3\text{V}$ or $5\text{V}$ (via onboard XC6206 LDO) | Powered from ESP32 $5\text{V}$ / VIN rail or $3.3\text{V}$ rail |
| **Max SPI Write Clock** | Up to $40\text{ MHz} - 50\text{ MHz}$ in write mode | Run at $27\text{ MHz} - 40\text{ MHz}$ on ESP32 VSPI |

---

## 2. 4-Wire SPI Hardware Interface & Pin Functions

The 4-wire serial interface uses a dedicated Data/Command (`D/C` or `RS`) select pin to differentiate between control register commands and payload pixel data:

| Pin Name | Direction | Signal Type | Function & Timing Rule |
|---|---|---|---|
| **`VCC`** | Power | $3.3\text{V} / 5\text{V}$ | Main power supply. If module has an onboard 3.3V LDO, use 5V to prevent LDO drop-out. |
| **`GND`** | Power | Ground ($0\text{V}$) | Common system ground return. |
| **`CS`** | Input | Active LOW | Chip Select. Must stay LOW during the entire byte transmission sequence. |
| **`RESET` (`RST`)**| Input | Active LOW | Hardware reset. Pulse LOW for $>10\ \mu\text{s}$, then wait $120\text{ ms}$ before software init. |
| **`DC` (`RS`)** | Input | Logic Level | **$0$ = Command byte** (register address), **$1$ = Parameter / Pixel data**. |
| **`SDI` (`MOSI`)** | Input | Serial Data | Serial data shifted in on the rising edge of `SCK` (MSB first). |
| **`SCK` (`CLK`)** | Input | Clock | Serial clock. SPI Mode 0 ($CPOL=0, CPHA=0$) or Mode 3 ($CPOL=1, CPHA=1$). |
| **`LED` (`BLK`)** | Input | Backlight Control| Backlight anode/transistor gate. High = ON, Low = OFF. Can be PWM modulated. |
| **`SDO` (`MISO`)** | Output | Serial Data | Read back GRAM or device ID (optional; left unconnected in write-only dashboards). |

---

## 3. Core ILI9341 Command Instruction Set

Writing to the display follows a two-stage protocol:
1. Pull `DC` LOW, assert `CS` LOW, send the **8-bit Command Code**.
2. Pull `DC` HIGH, stream **Parameter / Data Bytes**, de-assert `CS` HIGH.

| Command Name | Opcode (Hex) | Parameter Count | Function Description |
|---|---|---|---|
| **`NOP`** | `0x00` | 0 | No operation. |
| **`SWRESET`** | `0x01` | 0 | Software reset. Clears registers to defaults. Requires $5\text{ ms}$ delay before next command. |
| **`RDDID`** | `0x04` | 3 | Read display identification (Manufacturer ID, Driver Version). |
| **`SLPIN`** | `0x10` | 0 | Enter sleep mode (lowers power consumption; oscillator stopped). |
| **`SLPOUT`** | `0x11` | 0 | Exit sleep mode. **Mandatory:** Must delay $120\text{ ms}$ for internal DC-DC charge pumps to stabilize. |
| **`DISPOFF`** | `0x28` | 0 | Blank display (output is black, GRAM contents preserved). |
| **`DISPON`** | `0x29` | 0 | Enable display output from frame memory. |
| **`CASET`** | `0x2A` | 4 | **Column Address Set:** Defines horizontal drawing window ($X_{\text{start}}$ to $X_{\text{end}}$). |
| **`PASET`** | `0x2B` | 4 | **Page Address Set:** Defines vertical drawing window ($Y_{\text{start}}$ to $Y_{\text{end}}$). |
| **`RAMWR`** | `0x2C` | Variable | **Memory Write:** Streams raw RGB565 pixel data into the window defined by `CASET` and `PASET`. |
| **`MADCTL`** | `0x36` | 1 | **Memory Access Control:** Configures scan directions (Rotation, Mirror, BGR/RGB order). |
| **`PIXFMT`** | `0x3A` | 1 | **Pixel Format Set:** `0x55` selects 16-bit/pixel (RGB565). `0x66` selects 18-bit/pixel. |

---

## 4. Window Addressing & Pixel Streaming Protocol

Every drawing operation (clearing a screen, drawing a bitmap, or rendering a font glyph) executes through the **Window Address Flow**:

```text
[Host CPU]                       [ILI9341 Controller]
    |                                     |
    |--- 1. Send CASET (0x2A) ----------->| Set column boundaries
    |       Params: [X_start_H, X_start_L, X_end_H, X_end_L]
    |                                     |
    |--- 2. Send PASET (0x2B) ----------->| Set page/row boundaries
    |       Params: [Y_start_H, Y_start_L, Y_end_H, Y_end_L]
    |                                     |
    |--- 3. Send RAMWR (0x2C) ----------->| Internal pointer resets to (X_start, Y_start)
    |                                     |
    |=== 4. Stream Pixel Stream =========>| Auto-increments column then row
    |       [Pixel_0_H, Pixel_0_L, Pixel_1_H, ...]
    |                                     |
```

### Memory Access Control (`MADCTL` - `0x36`) Bit Definitions:
- **Bit 7 (`MY`):** Row Address Order ($0$ = Top-to-Bottom, $1$ = Bottom-to-Top).
- **Bit 6 (`MX`):** Column Address Order ($0$ = Left-to-Right, $1$ = Right-to-Left).
- **Bit 5 (`MV`):** Row/Column Exchange ($0$ = Normal Portrait, $1$ = Landscape Transpose).
- **Bit 3 (`BGR`):** Color Filter Order ($0$ = RGB, $1$ = BGR). *If colors look blue instead of red, flip this bit.*

---

## 5. Color Representation: 16-bit RGB565

In 16-bit mode, each pixel occupies exactly 2 bytes ($16\text{ bits}$):
- **Red:** 5 bits (values $0$ to $31$) $\rightarrow$ bits `[15:11]`
- **Green:** 6 bits (values $0$ to $63$) $\rightarrow$ bits `[10:5]`
- **Blue:** 5 bits (values $0$ to $31$) $\rightarrow$ bits `[4:0]`

$$\text{RGB565} = \left(\left(R \ \& \ 0xF8\right) \ll 8\right) \ | \ \left(\left(G \ \& \ 0xFC\right) \ll 3\right) \ | \ \left(B \gg 3\right)$$

### Endianness Caveat on SPI:
The SPI bus transmits the Most Significant Byte first. If writing directly via raw SPI buffers, the 16-bit integer `0xF800` (Pure Red) must be transmitted as high byte `0xF8` followed by low byte `0x00`.

---

## 6. Rendering Optimization & Anti-Flicker Architecture

A full screen refresh at $320 \times 240$ resolution requires:
$$320 \times 240 \times 2\text{ bytes} = 153,600\text{ bytes} \ (\approx 150\text{ KB})$$

At an SPI clock speed of $27\text{ MHz}$:
$$T_{\text{full\_frame}} = \frac{153,600 \times 8\text{ bits}}{27 \times 10^6\text{ bps}} \approx 45.5\text{ ms} \ (\approx 22\text{ FPS})$$

### Why `tft.fillScreen()` Causes Severe Flicker:
If an application clears the screen (`fillScreen(BLACK)`) every loop iteration before redrawing text, the viewer perceives a strobe/flash effect because clearing takes $\sim 45\text{ ms}$ and drawing takes another $\sim 30\text{ ms}$.

### Mitigation Strategies:
1. **Dirty Rectangle / Bounding Box Overwrite:**
   Instead of clearing the screen, erase only the bounding box of the changing telemetry value using `tft.fillRect(x, y, w, h, BG_COLOR)` or by using Adafruit GFX text background overwrite:
   ```cpp
   tft.setTextColor(ILI9341_GREEN, ILI9341_BLACK); // Overwrites previous characters in black
   ```
2. **Double-Buffered Fast Canvas (Spriting):**
   Allocate a small SRAM buffer (e.g., $120 \times 30$ pixels for the pitch/roll meter $\approx 7.2\text{ KB}$), draw in memory, and push to display via SPI in a single burst:
   ```cpp
   tft.drawRGBBitmap(x, y, buffer, w, h);
   ```

---

## 7. Backlight Control & Troubleshooting

The backlight circuit consists of 4 to 6 white LEDs wired in parallel, drawing between $40\text{ mA}$ and $90\text{ mA}$.
- **Direct 3.3V connection:** Permanently ON at 100% brightness.
- **GPIO Direct Drive:** Only permissible if the display board includes an onboard NPN/MOSFET buffer transistor. Connecting raw LED cathodes directly to an ESP32 GPIO can exceed the pin's $40\text{ mA}$ absolute maximum rating.
- **PWM Dimming:** Connect to an ESP32 LEDC channel configured at $5\text{ kHz}$ to prevent audible coil whine and display flicker.
