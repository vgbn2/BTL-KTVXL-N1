/**
 * @file uart.h
 * @brief Standalone Register-Level UART Driver for STM32F103 (Blue Pill)
 * Reference: ST RM0008 Reference Manual (DocID13902 Rev 21)
 *
 * Supported peripherals & default pins:
 * - USART1: TX = PA9,  RX = PA10 (APB2 bus)
 * - USART2: TX = PA2,  RX = PA3  (APB1 bus)
 * - USART3: TX = PB10, RX = PB11 (APB1 bus)
 *
 * Datasheet Verification:
 * - Table 26 (USART pinout): TX is AF Push-Pull (0x0B), RX is Input Floating/Pull-up (0x04/0x08).
 * - Section 27.3.4 (Fractional Baud Rate): USARTDIV = f_PCLK / (16 * Baud).
 * - Dynamic Clock Awareness: Automatically detects 8MHz HSI or 72MHz HSE+PLL via RCC_CFGR.
 * - Section 27.6.1: ORE (Overrun error) and RXNE safely cleared via SR -> DR read sequence.
 */

#ifndef UART_H
#define UART_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    UART_1 = 0, /* USART1: TX=PA9,  RX=PA10 (APB2) */
    UART_2 = 1, /* USART2: TX=PA2,  RX=PA3  (APB1) */
    UART_3 = 2, /* USART3: TX=PB10, RX=PB11 (APB1) */
    UART_PORT_COUNT = 3
} uart_port_t;

typedef enum {
    UART_OK               =  0,
    UART_ERR_INVALID_PORT = -1,
    UART_ERR_INVALID_BAUD = -2,
    UART_ERR_BUFFER_FULL  = -3,
    UART_ERR_NO_DATA      = -4
} uart_status_t;

#define UART_RX_BUFFER_SIZE 128

/* ========================================================================== */
/*                               CORE DRIVER API                              */
/* ========================================================================== */

/**
 * @brief Initialize UART port with specified baudrate (8N1).
 * Automatically senses current peripheral bus clock (HSI 8MHz or HSE/PLL 72MHz).
 */
uart_status_t uart_init(uart_port_t port, uint32_t baudrate);

/**
 * @brief Transmit a single character over UART (blocks until TXE flag is set).
 */
void uart_putc(uart_port_t port, char c);

/**
 * @brief Transmit a null-terminated string. Automatically expands '\n' to '\r\n'.
 */
void uart_puts(uart_port_t port, const char *str);

/**
 * @brief Transmit raw binary buffer over UART.
 */
void uart_write(uart_port_t port, const uint8_t *data, uint16_t len);

/**
 * @brief Check if at least one received character is waiting in the RX buffer.
 */
bool uart_available(uart_port_t port);

/**
 * @brief Read single byte from RX buffer.
 * @return 0..255 byte value, or -1 if no data is available.
 */
int16_t uart_getc(uart_port_t port);

/**
 * @brief Read up to max_len bytes into buffer.
 * @return Number of bytes actually read.
 */
uint16_t uart_read(uart_port_t port, uint8_t *buffer, uint16_t max_len);

/**
 * @brief Read a line into buffer until '\n' or '\r'. Line is null-terminated.
 * @return Length of received line (excluding null terminator).
 */
uint16_t uart_read_line(uart_port_t port, char *buffer, uint16_t max_len);

/**
 * @brief Clear and flush all received bytes from RX buffer.
 */
void uart_flush(uart_port_t port);

/* ========================================================================== */
/*                          LIGHTWEIGHT FORMATTING                            */
/* ========================================================================== */

/**
 * @brief Transmit signed 32-bit integer in decimal format (e.g. -12345).
 */
void uart_print_int(uart_port_t port, int32_t val);

/**
 * @brief Transmit unsigned 32-bit integer as hex (e.g. 0x08000000).
 */
void uart_print_hex(uart_port_t port, uint32_t val);

/**
 * @brief Transmit float value with fixed decimal precision.
 */
void uart_print_float(uart_port_t port, float val, uint8_t decimals);

/* ========================================================================== */
/*                             CLOCK & HARDWARE                               */
/* ========================================================================== */

/**
 * @brief Switch STM32F103 system clock to 72MHz using 8MHz HSE crystal + 9x PLL.
 * Automatically configures Flash 2 wait states and APB prescalers.
 * Returns true if locked to 72MHz PLL, false if HSE timed out (falls back to HSI).
 */
bool system_clock_init_72mhz(void);

/**
 * @brief Inspect RCC registers to return the actual clock frequency (Hz) for this UART.
 */
uint32_t uart_get_pclk(uart_port_t port);

/**
 * @brief Compute 16-bit USART_BRR register value from clock and baudrate.
 */
uint16_t uart_calculate_brr(uint32_t pclk, uint32_t baudrate);

/**
 * @brief Interrupt service routine handler (called from USARTx_IRQHandler).
 */
void uart_isr_handler(uart_port_t port);

/* ========================================================================== */
/*                         PRIMARY USART1 SHORTCUTS                           */
/* ========================================================================== */

static inline uart_status_t uart1_init(uint32_t baudrate) { return uart_init(UART_1, baudrate); }
static inline void          uart1_putc(char c)            { uart_putc(UART_1, c); }
static inline void          uart1_puts(const char *str)   { uart_puts(UART_1, str); }
static inline bool          uart1_available(void)         { return uart_available(UART_1); }
static inline int16_t       uart1_getc(void)              { return uart_getc(UART_1); }
static inline void          uart1_print_int(int32_t v)    { uart_print_int(UART_1, v); }
static inline void          uart1_print_hex(uint32_t v)   { uart_print_hex(UART_1, v); }
static inline void          uart1_print_float(float v, uint8_t d) { uart_print_float(UART_1, v, d); }

#if !defined(__arm__) || defined(UNIT_TEST)
/* Simulation hooks for host unit testing */
void     uart_sim_inject_rx(uart_port_t port, const uint8_t *data, uint16_t len);
uint16_t uart_sim_get_tx(uart_port_t port, uint8_t *buffer, uint16_t max_len);
void     uart_sim_reset(void);
#endif

#ifdef __cplusplus
}
#endif

#endif /* UART_H */
