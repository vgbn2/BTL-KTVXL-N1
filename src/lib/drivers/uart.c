/**
 * @file uart.c
 * @brief Standalone Register-Level UART Driver Implementation for STM32F103
 *
 * Source References:
 * - ST RM0008 Reference Manual (DocID13902 Rev 21):
 *   - Section 3.3.3: Flash access control register (FLASH_ACR) & Table 3 latency.
 *   - Section 3.3: Memory organization & Table 4 peripheral register boundaries.
 *   - Section 7.3: Reset and clock control (RCC) registers (CR, CFGR, APB1ENR, APB2ENR).
 *   - Section 9.1.2 & Table 26: USART GPIO alternate function configurations.
 *   - Section 9.2: GPIO register maps (CRL, CRH, BSRR, BRR).
 *   - Section 27.3.4: Fractional baud rate generation (USARTDIV formula).
 *   - Section 27.6: USART registers (SR, DR, BRR, CR1, CR2, CR3).
 */

#include "uart.h"
#include <stddef.h>

#if defined(__arm__) && !defined(UNIT_TEST)

/* ========================================================================== */
/*                        STM32F103 REGISTER MAP DEFINITIONS                  */
/* ========================================================================== */

#define REG32(addr)                 (*(volatile uint32_t *)(addr))

/* --- Flash Access Control Register (RM0008 Section 3.3.3) --- */
#define FLASH_BASE                  0x40022000u           /* RM0008 Section 3.3 Table 4: Flash memory interface */
#define FLASH_ACR                   REG32(FLASH_BASE + 0x00u) /* Offset 0x00 */
#define FLASH_ACR_LATENCY_2WS       (0x02u)               /* RM0008 Section 3.3.3 Table 3: Two wait states (48MHz < SYSCLK <= 72MHz) */
#define FLASH_ACR_PRFTBE            (1u << 4)             /* RM0008 Section 3.3.3 Bit 4: Prefetch buffer enable */

/* --- RCC Registers (RM0008 Section 7.3) --- */
#define RCC_BASE                    0x40021000u           /* RM0008 Section 3.3 Table 4: RCC base */
#define RCC_CR                      REG32(RCC_BASE + 0x00u) /* RM0008 Section 7.3.1: Clock control register */
#define RCC_CFGR                    REG32(RCC_BASE + 0x04u) /* RM0008 Section 7.3.2: Clock configuration register */
#define RCC_APB2ENR                 REG32(RCC_BASE + 0x18u) /* RM0008 Section 7.3.7: APB2 peripheral clock enable register */
#define RCC_APB1ENR                 REG32(RCC_BASE + 0x1Cu) /* RM0008 Section 7.3.8: APB1 peripheral clock enable register */

/* --- GPIO Port Registers (RM0008 Section 9.2) --- */
#define GPIOA_BASE                  0x40010800u           /* RM0008 Section 3.3 Table 4: GPIOA base */
#define GPIOA_CRL                   REG32(GPIOA_BASE + 0x00u) /* RM0008 Section 9.2.1: Port configuration register low (pins 0..7) */
#define GPIOA_CRH                   REG32(GPIOA_BASE + 0x04u) /* RM0008 Section 9.2.2: Port configuration register high (pins 8..15) */

#define GPIOB_BASE                  0x40010C00u           /* RM0008 Section 3.3 Table 4: GPIOB base */
#define GPIOB_CRL                   REG32(GPIOB_BASE + 0x00u) /* RM0008 Section 9.2.1 */
#define GPIOB_CRH                   REG32(GPIOB_BASE + 0x04u) /* RM0008 Section 9.2.2 */

/* --- USART Peripheral Base Addresses (RM0008 Section 3.3 Table 4) --- */
#define USART1_BASE                 0x40013800u           /* RM0008 Section 3.3 Table 4: USART1 base on APB2 */
#define USART2_BASE                 0x40004400u           /* RM0008 Section 3.3 Table 4: USART2 base on APB1 */
#define USART3_BASE                 0x40004800u           /* RM0008 Section 3.3 Table 4: USART3 base on APB1 */

/* --- USART Register Offsets (RM0008 Section 27.6 Table 198) --- */
#define USART_SR(base)              REG32((base) + 0x00u) /* RM0008 Section 27.6.1: Status register */
#define USART_DR(base)              REG32((base) + 0x04u) /* RM0008 Section 27.6.2: Data register */
#define USART_BRR(base)             REG32((base) + 0x08u) /* RM0008 Section 27.6.3: Baud rate register */
#define USART_CR1(base)             REG32((base) + 0x0Cu) /* RM0008 Section 27.6.4: Control register 1 */
#define USART_CR2(base)             REG32((base) + 0x10u) /* RM0008 Section 27.6.5: Control register 2 */
#define USART_CR3(base)             REG32((base) + 0x14u) /* RM0008 Section 27.6.6: Control register 3 */

/* --- USART Status Register Bit Flags (RM0008 Section 27.6.1) --- */
#define USART_SR_TXE                (1u << 7)             /* RM0008 Section 27.6.1 Bit 7: Transmit data register empty */
#define USART_SR_TC                 (1u << 6)             /* RM0008 Section 27.6.1 Bit 6: Transmission complete */
#define USART_SR_RXNE               (1u << 5)             /* RM0008 Section 27.6.1 Bit 5: Read data register not empty */
#define USART_SR_ORE                (1u << 3)             /* RM0008 Section 27.6.1 Bit 3: Overrun error */

/* --- USART Control Register 1 Bit Flags (RM0008 Section 27.6.4) --- */
#define USART_CR1_UE                (1u << 13)            /* RM0008 Section 27.6.4 Bit 13: USART enable */
#define USART_CR1_RXNEIE            (1u << 5)             /* RM0008 Section 27.6.4 Bit 5: RXNE interrupt enable */
#define USART_CR1_TE                (1u << 3)             /* RM0008 Section 27.6.4 Bit 3: Transmitter enable */
#define USART_CR1_RE                (1u << 2)             /* RM0008 Section 27.6.4 Bit 2: Receiver enable */

/* --- RCC Peripheral Clock Enable Bits --- */
#define RCC_APB2_AFIOEN             (1u << 0)             /* RM0008 Section 7.3.7 Bit 0: Alternate function IO clock enable */
#define RCC_APB2_IOPAEN             (1u << 2)             /* RM0008 Section 7.3.7 Bit 2: I/O port A clock enable */
#define RCC_APB2_IOPBEN             (1u << 3)             /* RM0008 Section 7.3.7 Bit 3: I/O port B clock enable */
#define RCC_APB2_USART1EN           (1u << 14)            /* RM0008 Section 7.3.7 Bit 14: USART1 clock enable */

#define RCC_APB1_USART2EN           (1u << 17)            /* RM0008 Section 7.3.8 Bit 17: USART2 clock enable */
#define RCC_APB1_USART3EN           (1u << 18)            /* RM0008 Section 7.3.8 Bit 18: USART3 clock enable */

static const uint32_t s_uart_bases[UART_PORT_COUNT] = {
    USART1_BASE,
    USART2_BASE,
    USART3_BASE
};

#endif /* __arm__ && !UNIT_TEST */

/* ========================================================================== */
/*                            CIRCULAR RING BUFFER                            */
/* ========================================================================== */

typedef struct {
    uint8_t  buffer[UART_RX_BUFFER_SIZE];
    volatile uint16_t head;
    volatile uint16_t tail;
} ring_buffer_t;

static ring_buffer_t s_rx_rings[UART_PORT_COUNT];

#if !defined(__arm__) || defined(UNIT_TEST)
static uint8_t  s_sim_tx[UART_PORT_COUNT][1024];
static uint16_t s_sim_tx_len[UART_PORT_COUNT];
#endif

static inline bool ring_is_empty(const ring_buffer_t *r) {
    return (r->head == r->tail);
}

static inline void ring_push(ring_buffer_t *r, uint8_t byte) {
    uint16_t next = (r->head + 1) % UART_RX_BUFFER_SIZE;
    if (next != r->tail) {
        r->buffer[r->head] = byte;
        r->head = next;
    }
}

static inline int16_t ring_pop(ring_buffer_t *r) {
    if (r->head == r->tail) {
        return -1;
    }
    uint8_t byte = r->buffer[r->tail];
    r->tail = (r->tail + 1) % UART_RX_BUFFER_SIZE;
    return (int16_t)byte;
}

/* ========================================================================== */
/*                             CLOCK MANAGEMENT                               */
/* ========================================================================== */

bool system_clock_init_72mhz(void) {
#if defined(__arm__) && !defined(UNIT_TEST)
    /* 1. Turn on HSE (8MHz crystal) - RM0008 Section 7.3.1: RCC_CR Bit 16 (HSEON) */
    RCC_CR |= (1u << 16);
    uint32_t timeout = 50000u;
    /* Wait for HSERDY - RM0008 Section 7.3.1: RCC_CR Bit 17 (HSERDY) */
    while (!(RCC_CR & (1u << 17)) && --timeout) {}
    if (timeout == 0) return false; /* HSE start failed, remain on 8MHz HSI */

    /* 2. Configure Flash Latency: 2 wait states for 72MHz - RM0008 Section 3.3.3 Table 3 */
    FLASH_ACR = FLASH_ACR_LATENCY_2WS | FLASH_ACR_PRFTBE;

    /* 3. Configure Clock Dividers in RCC_CFGR (RM0008 Section 7.3.2):
     *    - AHB Prescaler: HPRE[7:4] = 0000b (SYSCLK not divided -> 72MHz)
     *    - APB2 Prescaler: PPRE2[13:11] = 000b (HCLK not divided -> 72MHz max)
     *    - APB1 Prescaler: PPRE1[10:8] = 100b (0x04: HCLK divided by 2 -> 36MHz max allowed)
     *    - PLL Source: PLLSRC bit 16 = 1 (HSE oscillator clock selected as PLL input)
     *    - PLL Multiplier: PLLMUL[21:18] = 0111b (0x07: PLL input clock x 9 -> 8MHz * 9 = 72MHz)
     */
    RCC_CFGR = (0x04u << 8) | (1u << 16) | (0x07u << 18);

    /* 4. Enable PLL - RM0008 Section 7.3.1: RCC_CR Bit 24 (PLLON) */
    RCC_CR |= (1u << 24);
    timeout = 50000u;
    /* Wait for PLLRDY - RM0008 Section 7.3.1: RCC_CR Bit 25 (PLLRDY) */
    while (!(RCC_CR & (1u << 25)) && --timeout) {}
    if (timeout == 0) return false;

    /* 5. Switch System Clock to PLL - RM0008 Section 7.3.2: RCC_CFGR Bits [1:0] SW = 10b (PLL selected) */
    RCC_CFGR |= 0x02u;
    timeout = 50000u;
    /* Wait until PLL is used as system clock - RM0008 Section 7.3.2: Bits [3:2] SWS == 10b (0x08) */
    while (((RCC_CFGR & 0x0Cu) != 0x08u) && --timeout) {}
    return (timeout > 0);
#else
    return true;
#endif
}

uint32_t uart_get_pclk(uart_port_t port) {
#if defined(__arm__) && !defined(UNIT_TEST)
    uint32_t cfgr = RCC_CFGR;
    /* RM0008 Section 7.3.2: Bits 3:2 SWS (System clock switch status) */
    uint32_t sws = (cfgr >> 2) & 0x03u;
    uint32_t sysclk = 8000000u; /* Default HSI internal 8MHz RC oscillator */

    if (sws == 0x01u) {
        /* SWS = 01b: HSE oscillator used as system clock (RM0008 Section 7.3.2) */
        sysclk = 8000000u;
    } else if (sws == 0x02u) {
        /* SWS = 10b: PLL used as system clock (RM0008 Section 7.3.2) */
        uint32_t pllsrc = (cfgr >> 16) & 0x01u;             /* Bit 16: PLLSRC */
        uint32_t pllmul = ((cfgr >> 18) & 0x0Fu) + 2u;     /* Bits 21:18: PLLMUL (0000b = x2 .. 0111b = x9) */
        if (pllmul > 16u) pllmul = 16u;
        uint32_t pllin = pllsrc ? 8000000u : 4000000u;      /* Bit 16=1: HSE (8MHz), Bit 16=0: HSI/2 (4MHz) */
        sysclk = pllin * pllmul;
    }

    /* AHB Prescaler - RM0008 Section 7.3.2 Bits 7:4 HPRE */
    uint32_t hpre = (cfgr >> 4) & 0x0Fu;
    uint32_t hclk = sysclk;
    if (hpre >= 0x08u) {
        /* 1000b: /2, 1001b: /4, 1010b: /8, 1011b: /16, 1100b: /64, 1101b: /128, 1110b: /256, 1111b: /512 */
        static const uint8_t ahb_shift[] = {1, 2, 3, 4, 6, 7, 8, 9};
        hclk >>= ahb_shift[hpre - 0x08u];
    }

    if (port == UART_1) {
        /* APB2 Prescaler (PCLK2) - RM0008 Section 7.3.2 Bits 13:11 PPRE2 */
        uint32_t ppre2 = (cfgr >> 11) & 0x07u;
        if (ppre2 >= 0x04u) {
            /* 100b: HCLK/2, 101b: HCLK/4, 110b: HCLK/8, 111b: HCLK/16 */
            return hclk >> (ppre2 - 0x03u);
        }
        return hclk; /* PPRE2 < 4: HCLK not divided */
    } else {
        /* APB1 Prescaler (PCLK1) - RM0008 Section 7.3.2 Bits 10:8 PPRE1 */
        uint32_t ppre1 = (cfgr >> 8) & 0x07u;
        if (ppre1 >= 0x04u) {
            /* 100b: HCLK/2, 101b: HCLK/4, 110b: HCLK/8, 111b: HCLK/16 */
            return hclk >> (ppre1 - 0x03u);
        }
        return hclk; /* PPRE1 < 4: HCLK not divided */
    }
#else
    return (port == UART_1) ? 72000000u : 36000000u;
#endif
}

uint16_t uart_calculate_brr(uint32_t pclk, uint32_t baudrate) {
    if (baudrate == 0 || pclk == 0) return 0;
    /*
     * RM0008 Section 27.3.4: Fractional baud rate generation
     * Formula:
     *   USARTDIV = f_PCLK / (16 * Baud)
     *   DIV_Mantissa = USARTDIV integer part = pclk / (16 * baudrate)
     *   DIV_Fraction = round(16 * remainder / (16 * baudrate))
     *
     * In USART_BRR register:
     *   Bits 15:4 = DIV_Mantissa (12 bits)
     *   Bits 3:0  = DIV_Fraction (4 bits)
     */
    uint32_t div = pclk / (16u * baudrate);
    uint32_t rem = pclk % (16u * baudrate);
    uint32_t frac = (rem * 16u + (8u * baudrate)) / (16u * baudrate);
    if (frac >= 16u) {
        div++;
        frac = 0;
    }
    return (uint16_t)((div << 4) | (frac & 0x0Fu));
}

/* ========================================================================== */
/*                             INITIALIZATION                                 */
/* ========================================================================== */

uart_status_t uart_init(uart_port_t port, uint32_t baudrate) {
    if (port >= UART_PORT_COUNT) return UART_ERR_INVALID_PORT;
    if (baudrate == 0) return UART_ERR_INVALID_BAUD;

    s_rx_rings[port].head = 0;
    s_rx_rings[port].tail = 0;

#if defined(__arm__) && !defined(UNIT_TEST)
    uint32_t base = s_uart_bases[port];
    uint32_t pclk = uart_get_pclk(port);

    if (port == UART_1) {
        /* Enable GPIOA, AFIO, and USART1 on APB2 (RM0008 Section 7.3.7) */
        RCC_APB2ENR |= (RCC_APB2_IOPAEN | RCC_APB2_USART1EN | RCC_APB2_AFIOEN);

        /*
         * RM0008 Section 9.1.2 Table 26 (USART1 pinout):
         *   PA9  = USART1_TX: Alternate function push-pull (MODE=11: 50MHz, CNF=10: AF PP -> 0x0B)
         *   PA10 = USART1_RX: Input floating (MODE=00: Input, CNF=01: Floating -> 0x04)
         *
         * RM0008 Section 9.2.2 (GPIOA_CRH):
         *   PA9  bits [7:4]
         *   PA10 bits [11:8]
         */
        GPIOA_CRH &= ~(0x0Fu << 4);
        GPIOA_CRH |=  (0x0Bu << 4);

        GPIOA_CRH &= ~(0x0Fu << 8);
        GPIOA_CRH |=  (0x04u << 8);
    } else if (port == UART_2) {
        /* Enable GPIOA on APB2, USART2 on APB1 (RM0008 Section 7.3.7 & 7.3.8) */
        RCC_APB2ENR |= (RCC_APB2_IOPAEN | RCC_APB2_AFIOEN);
        RCC_APB1ENR |= RCC_APB1_USART2EN;

        /*
         * RM0008 Section 9.1.2 Table 26 (USART2 pinout):
         *   PA2 = USART2_TX: AF push-pull 50MHz (MODE=11, CNF=10 -> 0x0B)
         *   PA3 = USART2_RX: Input floating (MODE=00, CNF=01 -> 0x04)
         *
         * RM0008 Section 9.2.1 (GPIOA_CRL):
         *   PA2 bits [11:8]
         *   PA3 bits [15:12]
         */
        GPIOA_CRL &= ~(0x0Fu << 8);
        GPIOA_CRL |=  (0x0Bu << 8);

        GPIOA_CRL &= ~(0x0Fu << 12);
        GPIOA_CRL |=  (0x04u << 12);
    } else if (port == UART_3) {
        /* Enable GPIOB on APB2, USART3 on APB1 (RM0008 Section 7.3.7 & 7.3.8) */
        RCC_APB2ENR |= (RCC_APB2_IOPBEN | RCC_APB2_AFIOEN);
        RCC_APB1ENR |= RCC_APB1_USART3EN;

        /*
         * RM0008 Section 9.1.2 Table 26 (USART3 pinout):
         *   PB10 = USART3_TX: AF push-pull 50MHz (MODE=11, CNF=10 -> 0x0B)
         *   PB11 = USART3_RX: Input floating (MODE=00, CNF=01 -> 0x04)
         *
         * RM0008 Section 9.2.2 (GPIOB_CRH):
         *   PB10 bits [11:8]
         *   PB11 bits [15:12]
         */
        GPIOB_CRH &= ~(0x0Fu << 8);
        GPIOB_CRH |=  (0x0Bu << 8);

        GPIOB_CRH &= ~(0x0Fu << 12);
        GPIOB_CRH |=  (0x04u << 12);
    }

    /* Set calculated baudrate in USART_BRR (RM0008 Section 27.6.3) */
    USART_BRR(base) = uart_calculate_brr(pclk, baudrate);

    /* 8N1: 1 stop bit (STOP[13:12] = 00b in CR2) - RM0008 Section 27.6.5 */
    USART_CR2(base) = 0;

    /* No flow control (CTSE=0, RTSE=0 in CR3) - RM0008 Section 27.6.6 */
    USART_CR3(base) = 0;

    /* Enable USART, Transmitter, and Receiver (RM0008 Section 27.6.4: CR1 UE, TE, RE) */
    USART_CR1(base) = (USART_CR1_UE | USART_CR1_TE | USART_CR1_RE);
#endif

    return UART_OK;
}

/* ========================================================================== */
/*                              TRANSMISSION                                  */
/* ========================================================================== */

void uart_putc(uart_port_t port, char c) {
    if (port >= UART_PORT_COUNT) return;

#if defined(__arm__) && !defined(UNIT_TEST)
    uint32_t base = s_uart_bases[port];
    /* Wait for TXE (Transmit data register empty) - RM0008 Section 27.6.1 Bit 7 */
    while ((USART_SR(base) & USART_SR_TXE) == 0) {}
    /* Write character to Data Register - RM0008 Section 27.6.2 */
    USART_DR(base) = (uint8_t)c;
#else
    if (s_sim_tx_len[port] < sizeof(s_sim_tx[port])) {
        s_sim_tx[port][s_sim_tx_len[port]++] = (uint8_t)c;
    }
#endif
}

void uart_puts(uart_port_t port, const char *str) {
    if (!str || port >= UART_PORT_COUNT) return;
    while (*str) {
        if (*str == '\n') {
            uart_putc(port, '\r');
        }
        uart_putc(port, *str++);
    }
}

void uart_write(uart_port_t port, const uint8_t *data, uint16_t len) {
    if (!data || port >= UART_PORT_COUNT) return;
    for (uint16_t i = 0; i < len; i++) {
        uart_putc(port, (char)data[i]);
    }
}

/* ========================================================================== */
/*                                RECEPTION                                   */
/* ========================================================================== */

static void uart_poll_hardware_rx(uart_port_t port) {
#if defined(__arm__) && !defined(UNIT_TEST)
    uint32_t base = s_uart_bases[port];
    uint32_t sr = USART_SR(base);
    /*
     * RM0008 Section 27.6.1:
     * RXNE (bit 5): Read data register not empty. Cleared by a read to USART_DR.
     * ORE (bit 3): Overrun error. Cleared by a software sequence: read to USART_SR
     * followed by a read to USART_DR.
     * Reading SR then DR below simultaneously handles both RXNE and clears ORE safely.
     */
    if (sr & (USART_SR_RXNE | USART_SR_ORE)) {
        uint8_t byte = (uint8_t)(USART_DR(base) & 0xFFu);
        ring_push(&s_rx_rings[port], byte);
    }
#else
    (void)port;
#endif
}

bool uart_available(uart_port_t port) {
    if (port >= UART_PORT_COUNT) return false;
    uart_poll_hardware_rx(port);
    return !ring_is_empty(&s_rx_rings[port]);
}

int16_t uart_getc(uart_port_t port) {
    if (port >= UART_PORT_COUNT) return -1;
    uart_poll_hardware_rx(port);
    return ring_pop(&s_rx_rings[port]);
}

uint16_t uart_read(uart_port_t port, uint8_t *buffer, uint16_t max_len) {
    if (!buffer || max_len == 0 || port >= UART_PORT_COUNT) return 0;
    uint16_t count = 0;
    while (count < max_len && uart_available(port)) {
        int16_t byte = uart_getc(port);
        if (byte < 0) break;
        buffer[count++] = (uint8_t)byte;
    }
    return count;
}

uint16_t uart_read_line(uart_port_t port, char *buffer, uint16_t max_len) {
    if (!buffer || max_len <= 1 || port >= UART_PORT_COUNT) return 0;
    uint16_t idx = 0;

    while (idx < (max_len - 1)) {
        if (!uart_available(port)) continue;
        int16_t c = uart_getc(port);
        if (c < 0) break;

        if (c == '\r') continue;
        if (c == '\n') break;

        buffer[idx++] = (char)c;
    }
    buffer[idx] = '\0';
    return idx;
}

void uart_flush(uart_port_t port) {
    if (port >= UART_PORT_COUNT) return;
    s_rx_rings[port].head = 0;
    s_rx_rings[port].tail = 0;
}

void uart_isr_handler(uart_port_t port) {
    if (port >= UART_PORT_COUNT) return;
#if defined(__arm__) && !defined(UNIT_TEST)
    uint32_t base = s_uart_bases[port];
    uint32_t sr = USART_SR(base);
    /* RM0008 Section 27.6.1: Clear RXNE/ORE via SR -> DR read sequence */
    if (sr & (USART_SR_RXNE | USART_SR_ORE)) {
        uint8_t byte = (uint8_t)(USART_DR(base) & 0xFFu);
        ring_push(&s_rx_rings[port], byte);
    }
#endif
}

/* ========================================================================== */
/*                          LIGHTWEIGHT FORMATTING                            */
/* ========================================================================== */

void uart_print_int(uart_port_t port, int32_t val) {
    char buf[12];
    int idx = 0;
    if (val == 0) {
        uart_putc(port, '0');
        return;
    }
    if (val < 0) {
        uart_putc(port, '-');
        val = -val;
    }
    while (val > 0) {
        buf[idx++] = (char)('0' + (val % 10));
        val /= 10;
    }
    for (int i = idx - 1; i >= 0; i--) {
        uart_putc(port, buf[i]);
    }
}

void uart_print_hex(uart_port_t port, uint32_t val) {
    static const char hex_digits[] = "0123456789ABCDEF";
    uart_puts(port, "0x");
    for (int i = 28; i >= 0; i -= 4) {
        uart_putc(port, hex_digits[(val >> i) & 0x0Fu]);
    }
}

void uart_print_float(uart_port_t port, float val, uint8_t decimals) {
    if (val < 0.0f) {
        uart_putc(port, '-');
        val = -val;
    }
    int32_t integer_part = (int32_t)val;
    uart_print_int(port, integer_part);

    if (decimals > 0) {
        uart_putc(port, '.');
        float remainder = val - (float)integer_part;
        for (uint8_t i = 0; i < decimals; i++) {
            remainder *= 10.0f;
            int digit = (int)remainder;
            uart_putc(port, (char)('0' + digit));
            remainder -= (float)digit;
        }
    }
}

/* ========================================================================== */
/*                             SIMULATION HOOKS                               */
/* ========================================================================== */

#if !defined(__arm__) || defined(UNIT_TEST)
void uart_sim_inject_rx(uart_port_t port, const uint8_t *data, uint16_t len) {
    if (port >= UART_PORT_COUNT || !data) return;
    for (uint16_t i = 0; i < len; i++) {
        ring_push(&s_rx_rings[port], data[i]);
    }
}

uint16_t uart_sim_get_tx(uart_port_t port, uint8_t *buffer, uint16_t max_len) {
    if (port >= UART_PORT_COUNT || !buffer || max_len == 0) return 0;
    uint16_t copied = (s_sim_tx_len[port] < max_len) ? s_sim_tx_len[port] : max_len;
    for (uint16_t i = 0; i < copied; i++) {
        buffer[i] = s_sim_tx[port][i];
    }
    return copied;
}

void uart_sim_reset(void) {
    for (int p = 0; p < UART_PORT_COUNT; p++) {
        s_rx_rings[p].head = 0;
        s_rx_rings[p].tail = 0;
        s_sim_tx_len[p] = 0;
    }
}
#endif
