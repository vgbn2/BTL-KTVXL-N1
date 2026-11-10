# UART Telemetry Protocol & Receiver State Machine Deep Dive

**Datasheet References:**
- **STM32:** STMicroelectronics RM0008 *Reference Manual*, DocID 13902 Rev 17, **Section 27: Universal Synchronous Asynchronous Receiver Transmitter (USART)**, pp. 784–826.
- **ESP32:** Espressif Systems *ESP32 Series Datasheet*, v4.4, **Section 4.1.3: Universal Asynchronous Receiver/Transmitter (UART)**, pp. 26–27.

---

## 1. Physical Hardware UART Configuration & Timing

### 1.1 Baud Rate Calculation (STM32 RM0008 Section 27.3.4)

On the STM32F103, `USART1` is clocked by the high-speed peripheral clock $f_{\text{PCLK2}} = 72\text{ MHz}$ (or $8\text{ MHz}$ on default HSI).
The baud rate divisor is programmed into the 16-bit **Baud Rate Register (`USART_BRR`)** at address `0x40013808`:

$$\text{USARTDIV} = \frac{f_{\text{PCLK2}}}{16 \times \text{BaudRate}}$$

For standard **115,200 baud** @ $72\text{ MHz}$ $f_{\text{PCLK2}}$:
$$\text{USARTDIV} = \frac{72,000,000}{16 \times 115,200} = 39.0625$$

The integer and fractional parts are encoded into `USART_BRR`:
- **Mantissa (`USART_BRR[15:4]`):** $\text{Integer part} = 39 = \text{0x27}$
- **Fraction (`USART_BRR[3:0]`):** $\text{Round}(0.0625 \times 16) = \text{Round}(1.0) = 1 = \text{0x1}$
- **Register Value:** `USART_BRR = 0x0271` (Baud rate error is $0.00\%$).

*(For $8\text{ MHz}$ HSI bring-up: $\text{USARTDIV} = \frac{8,000,000}{16 \times 115,200} = 4.340 \implies \text{Mantissa}=4, \text{Fraction}=5 \implies \text{BRR}=0x0045$, error $+0.16\%$).*

---

### 1.2 Hardware FIFO & Bit Framing (ESP32 Datasheet Section 4.1.3)

| Parameter | Specification | Hardware Role |
|---|---|---|
| **Data Bits** | 8 bits (LSB transmitted first) | Standard byte transmission. |
| **Parity** | None (`USART_CR1[10] = 0`) | Parity bit omitted for maximum throughput. |
| **Stop Bits** | 1 Stop bit (`USART_CR2[13:12] = 00`) | Standard stop bit width. |
| **Bit Duration ($T_{\text{bit}}$)** | $1 / 115200 \approx 8.68\ \mu\text{s}$ | Sampling occurs on 8th, 9th, and 10th internal clock ticks. |
| **ESP32 RX Hardware FIFO** | 128 bytes deep | Absorbs incoming bursts during high-latency SPI display draw calls. |

---

## 2. Binary Telemetry Packet Structure (15 Bytes Fixed)

To eliminate parsing overhead and string fragmentation in embedded C, all telemetry is transmitted as a fixed-length binary frame:

```text
 Byte 0   Byte 1   Byte 2   Byte 3   Byte 4   Byte 5-6   Byte 7-8   Byte 9-10  Byte 11  Byte 12  Byte 13-14
+--------+--------+--------+--------+--------+----------+----------+----------+--------+--------+-----------+
|  0xAA  |  0x55  |  0x0F  | Sequence| Frame  |  Pitch   |   Roll   |   Yaw    | Status | Reserve| CRC16-    |
| Header1| Header2| Length | Number  | Type   |  (x100)  |  (x100)  |  (x100)  | Flags  | 0x00   | CCITT     |
+--------+--------+--------+--------+--------+----------+----------+----------+--------+--------+-----------+
```

### Detailed Field Definitions

| Offset | Name | Type | Unit / Encoding | Description |
|---|---|---|---|---|
| **`0`** | `SYNC_0` | `uint8_t` | Constant `0xAA` | Preamble synchronizer byte 1. |
| **`1`** | `SYNC_1` | `uint8_t` | Constant `0x55` | Preamble synchronizer byte 2. |
| **`2`** | `LEN` | `uint8_t` | `0x0F` (15 decimal) | Total frame length in bytes including CRC. |
| **`3`** | `SEQ` | `uint8_t` | $0 - 255$ | Sequence index. Increments on every packet to detect packet drops. |
| **`4`** | `TYPE` | `uint8_t` | Enum | `0x01` = Periodic Telemetry, `0x02` = Calibration Data, `0x03` = Alarm. |
| **`5-6`** | `PITCH` | `int16_t` | $0.01^\circ$ (Signed) | Pitch angle ($\pm 180.00^\circ \rightarrow -18000 \text{ to } +18000$, Little-Endian). |
| **`7-8`** | `ROLL` | `int16_t` | $0.01^\circ$ (Signed) | Roll angle ($\pm 180.00^\circ \rightarrow -18000 \text{ to } +18000$, Little-Endian). |
| **`9-10`**| `YAW` | `int16_t` | $0.01^\circ$ (Signed) | Yaw angle / heading ($0.00^\circ \text{ to } 360.00^\circ$, Little-Endian). |
| **`11`** | `STATUS`| `uint8_t` | Bitmask | Bit 0: IMU OK, Bit 1: Calibrated, Bit 2: Flash OK, Bit 3: Alarm. |
| **`12`** | `RESERVE`| `uint8_t` | `0x00` | Reserved for alignment / future expansion. |
| **`13-14`**| `CRC16`| `uint16_t`| CRC16-CCITT | Checksum over bytes 0 to 12 (Polynomial `0x1021`, Init `0xFFFF`). |

---

## 3. Mathematical Data Integrity: CRC16-CCITT

The checksum protects against electromagnetic noise, cable bounce, and ground loop jitter.

- **Polynomial:** $x^{16} + x^{12} + x^5 + 1$ (Hex `0x1021`)
- **Initial Value:** `0xFFFF`
- **Reflected In/Out:** False
- **XOR Output:** `0x0000`

### Implementation in C (Deterministic Tableless Algorithm):
```c
uint16_t crc16_ccitt(const uint8_t *data, uint32_t len) {
    uint16_t crc = 0xFFFFu;
    for (uint32_t i = 0; i < len; i++) {
        crc ^= (uint16_t)data[i] << 8;
        for (uint8_t bit = 0; bit < 8; bit++) {
            if (crc & 0x8000u) {
                crc = (uint16_t)((crc << 1) ^ 0x1021u);
            } else {
                crc = (uint16_t)(crc << 1);
            }
        }
    }
    return crc;
}
```

---

## 4. Receiver Finite State Machine (FSM)

On the ESP32 receiver, incoming UART bytes are ingested via a deterministic **5-State Moore Machine**. This guarantees auto-recovery from noisy or truncated transmissions without locking the CPU:

```text
                     +-----------------------------------+
                     |                                   |
                     v                                   |
               +-------------+                           |
      +------> | STATE_SYNC1 | (Wait for 0xAA)           |
      |        +------+------+                           |
      |               | Byte == 0xAA                     |
      |               v                                  |
      |        +-------------+                           |
 Byte != 0x55  | STATE_SYNC2 | (Check for 0x55)          |
      +------- +------+------+                           |
                      | Byte == 0x55                     |
                      v                                  |
               +-------------+                           |
 Length != 15  |  STATE_LEN  | (Check Length == 15)      |
      +------- +------+------+                           |
      |               | Byte == 15                       |
      |               v                                  |
      |        +-------------+                           |
      |        | STATE_BODY  | (Ingest 12 Payload Bytes) |
      |        +------+------+                           |
      |               | Count == 12                      |
      |               v                                  |
      |        +-------------+                           |
      +------- |  STATE_CRC  | (Verify CRC16 Checksum)   |
  Bad CRC      +------+------+                           |
                      | Valid CRC                        |
                      v                                  |
               [ DISPATCH TO ] --------------------------+
               [  DASHBOARD  ]
```

### Complete C++ Implementation on ESP32:
```cpp
enum RxState { STATE_SYNC1, STATE_SYNC2, STATE_LEN, STATE_BODY, STATE_CRC };

static RxState rx_state = STATE_SYNC1;
static uint8_t rx_buffer[15];
static uint8_t rx_index = 0;

void processSerialByte(uint8_t byte) {
    switch (rx_state) {
        case STATE_SYNC1:
            if (byte == 0xAA) {
                rx_buffer[0] = byte;
                rx_state = STATE_SYNC2;
            }
            break;

        case STATE_SYNC2:
            if (byte == 0x55) {
                rx_buffer[1] = byte;
                rx_state = STATE_LEN;
            } else {
                rx_state = (byte == 0xAA) ? STATE_SYNC2 : STATE_SYNC1;
            }
            break;

        case STATE_LEN:
            if (byte == 15) {
                rx_buffer[2] = byte;
                rx_index = 3;
                rx_state = STATE_BODY;
            } else {
                rx_state = STATE_SYNC1;
            }
            break;

        case STATE_BODY:
            rx_buffer[rx_index++] = byte;
            if (rx_index == 15) {
                /* Verify CRC16 */
                uint16_t received_crc = rx_buffer[13] | (rx_buffer[14] << 8);
                uint16_t computed_crc = crc16_ccitt(rx_buffer, 13);
                
                if (received_crc == computed_crc) {
                    /* Valid Telemetry Packet -> Update Dashboard */
                    int16_t pitch = (int16_t)(rx_buffer[5] | (rx_buffer[6] << 8));
                    int16_t roll  = (int16_t)(rx_buffer[7] | (rx_buffer[8] << 8));
                    updateDashboard(pitch / 100.0f, roll / 100.0f);
                }
                rx_state = STATE_SYNC1;
            }
            break;

        default:
            rx_state = STATE_SYNC1;
            break;
    }
}
```
