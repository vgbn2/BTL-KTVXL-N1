/**
 * @file test_uart.c
 * @brief Host unit test suite for STM32F103 UART driver and RM0008 calculations
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "uart.h"

int test_uart_module(void) {
    int failed = 0;

    printf("  [RUN] test_uart_calculate_brr (RM0008 Section 27.3.4)\n");
    /*
     * RM0008 Section 27.3.4 Example 1:
     * If f_PCLK = 72MHz, Baud = 115200:
     *   USARTDIV = 72000000 / (16 * 115200) = 39.0625
     *   Mantissa = 39 (0x27)
     *   Fraction = round(0.0625 * 16) = 1 (0x01)
     *   BRR = 0x0271
     */
    uint16_t brr_72m_115200 = uart_calculate_brr(72000000u, 115200u);
    if (brr_72m_115200 != 0x0271) {
        printf("    FAIL: brr_72m_115200 expected 0x0271, got 0x%04X\n", brr_72m_115200);
        failed++;
    }

    /*
     * If f_PCLK = 8MHz (default HSI), Baud = 115200:
     *   USARTDIV = 8000000 / (16 * 115200) = 4.340277...
     *   Mantissa = 4 (0x04)
     *   Fraction = round(0.340277... * 16) = round(5.44) = 5 (0x05)
     *   BRR = 0x0045
     */
    uint16_t brr_8m_115200 = uart_calculate_brr(8000000u, 115200u);
    if (brr_8m_115200 != 0x0045) {
        printf("    FAIL: brr_8m_115200 expected 0x0045, got 0x%04X\n", brr_8m_115200);
        failed++;
    }

    /*
     * If f_PCLK = 36MHz (APB1 max for USART2/3), Baud = 9600:
     *   USARTDIV = 36000000 / (16 * 9600) = 234.375
     *   Mantissa = 234 (0xEA)
     *   Fraction = round(0.375 * 16) = 6 (0x06)
     *   BRR = 0x0EA6
     */
    uint16_t brr_36m_9600 = uart_calculate_brr(36000000u, 9600u);
    if (brr_36m_9600 != 0x0EA6) {
        printf("    FAIL: brr_36m_9600 expected 0x0EA6, got 0x%04X\n", brr_36m_9600);
        failed++;
    }

    /* Edge case: Baud 0 or PCLK 0 returns 0 */
    if (uart_calculate_brr(0, 115200) != 0) failed++;
    if (uart_calculate_brr(72000000, 0) != 0) failed++;

    printf("  [RUN] test_uart_init_and_sim_io\n");
    uart_sim_reset();
    if (uart_init(UART_PORT_COUNT, 115200) != UART_ERR_INVALID_PORT) failed++;
    if (uart_init(UART_1, 0) != UART_ERR_INVALID_BAUD) failed++;
    if (uart_init(UART_1, 115200) != UART_OK) failed++;

    /* Test TX */
    uart_puts(UART_1, "Hello\n");
    uint8_t tx_buf[32];
    uint16_t tx_len = uart_sim_get_tx(UART_1, tx_buf, sizeof(tx_buf));
    if (tx_len != 7) { /* "Hello\r\n" = 7 bytes */
        printf("    FAIL: tx_len expected 7, got %u\n", tx_len);
        failed++;
    }
    if (memcmp(tx_buf, "Hello\r\n", 7) != 0) {
        printf("    FAIL: tx_buf content mismatch\n");
        failed++;
    }

    /* Test RX ring buffer */
    const uint8_t rx_test_data[] = "PONG\n";
    uart_sim_inject_rx(UART_1, rx_test_data, 5);
    if (!uart_available(UART_1)) failed++;

    char line[32];
    uint16_t line_len = uart_read_line(UART_1, line, sizeof(line));
    if (line_len != 4 || strcmp(line, "PONG") != 0) {
        printf("    FAIL: line expected 'PONG', got '%s' (len %u)\n", line, line_len);
        failed++;
    }

    /* Formatting tests */
    uart_sim_reset();
    uart_print_int(UART_1, -42);
    tx_len = uart_sim_get_tx(UART_1, tx_buf, sizeof(tx_buf));
    tx_buf[tx_len] = '\0';
    if (strcmp((char *)tx_buf, "-42") != 0) failed++;

    uart_sim_reset();
    uart_print_hex(UART_1, 0x12AB);
    tx_len = uart_sim_get_tx(UART_1, tx_buf, sizeof(tx_buf));
    tx_buf[tx_len] = '\0';
    if (strcmp((char *)tx_buf, "0x000012AB") != 0) failed++;

    return failed;
}

#if defined(STANDALONE_TEST)
int main(void) {
    int res = test_uart_module();
    if (res == 0) {
        printf("All UART driver unit tests passed cleanly! (0 failures)\n");
        return 0;
    }
    printf("UART driver tests failed: %d\n", res);
    return 1;
}
#endif
