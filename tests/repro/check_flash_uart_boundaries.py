"""Compile actual JEDEC predicate and RX ISR slices; no physical bus qualification."""
from pathlib import Path
import subprocess
import sys
import tempfile

root = Path(__file__).resolve().parents[2]
version = sys.argv[1] if len(sys.argv) > 1 else 'src'
bases = {'src': 'src', 'outer': 'BTLVXL-Nhóm 1/USER',
         'new': 'BTLVXL-Nhóm 1/BTLVXL-Nhóm 1/USER'}
base = root / bases[version]
flash = (base / ('lib/drivers/flash_config.c' if version == 'src' else 'flash_config.c')).read_text()
uart = (base / ('lib/drivers/uart_cmd.c' if version == 'src' else 'uart_cmd.c')).read_text()
start = flash.index('    if (', flash.index('select_flash(0);', flash.index('manufacturer =')))
predicate = flash[start:flash.index('    flash_ready = 1;', start)]
start = uart.index('void USART1_IRQHandler(void)')
end = uart.index('{', start)
depth = 1
cursor = end + 1
while depth:
    depth += (uart[cursor] == '{') - (uart[cursor] == '}')
    cursor += 1
isr = uart[start:cursor]
source = r'''
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#define IMU_ERR_FLASH -4
#define USART_SR_RXNE 32U
#define USART_SR_ORE 8U
#define USART_SR_FE 2U
#define USART_SR_NE 4U
#define RX_BUFFER_SIZE 128U
#define QUEUE_SIZE 4U
#define ECHO_BUFFER_SIZE 128U
#define IS_SRC __IS_SRC__
#define __DMB() ((void)0)
static struct { uint32_t SR, DR; } port;
#define UART_PORT (&port)
static volatile char commands[QUEUE_SIZE][RX_BUFFER_SIZE];
static volatile char echo_buf[ECHO_BUFFER_SIZE];
static volatile uint8_t queue_write, queue_read, echo_head, echo_tail, receive_error;
static uint16_t rx_index;
static uint8_t discard_line, last_was_cr, esc_state;
static int accepts(unsigned manufacturer, unsigned type, unsigned capacity, int spi_error) {
PREDICATE
return 0;
}
ISR
static void reset(void) {
    queue_write = queue_read = echo_head = echo_tail = receive_error = 0;
    rx_index = 0;
    discard_line = last_was_cr = esc_state = 0;
    memset((void *)commands, 0, sizeof commands);
    memset((void *)echo_buf, 0, sizeof echo_buf);
}
static void feed(const char *s) {
    while (*s) { port.SR = USART_SR_RXNE; port.DR = (unsigned char)*s++; USART1_IRQHandler(); }
}
static void drain_echo(char *out, size_t cap) {
    size_t k = 0;
    while (echo_tail != echo_head && k + 1 < cap) {
        out[k++] = echo_buf[echo_tail];
        echo_tail = (uint8_t)((echo_tail + 1U) % ECHO_BUFFER_SIZE);
    }
    out[k] = '\0';
}
int main(void) {
    unsigned mismatch = 0, total = 0;
    for (unsigned m=0; m<256; ++m)
        for (unsigned t=0; t<256; ++t)
            for (unsigned c=0; c<256; ++c) {
                int expected = (m==239 && (t==64 || t==96) && c>=20 && c<=24)
                               || (m==32 && t==64 && c==22);
                mismatch += ((accepts(m,t,c,0)==0) != expected);
                total++;
            }
    printf("JEDEC: %u inputs, %u mismatches; 204016 result=%d\n",total,mismatch,accepts(32,64,22,0));
    unsigned errors = mismatch;
    for (unsigned t=64; t<=96; t+=32)
        for (unsigned c=20; c<=24; ++c) errors += accepts(239,t,c,1)==0;
    errors += accepts(32,64,22,1)==0;
    const char *lines[] = {"GET_ANGLE\r", "GET_ANGLE\n", "GET_ANGLE\r\n"};
    for (unsigned i=0; i<3; ++i) {
        reset(); feed(lines[i]);
        int ok = queue_write==1 && !strcmp((const char *)commands[0],"GET_ANGLE");
        printf("UART terminator case %u: %s\n",i,ok?"PASS":"FAIL"); errors += !ok;
    }
    reset(); feed("\r\n\n"); errors += queue_write!=0;
    reset(); feed("A\rB\nC\r\nD\n");
    errors += queue_write!=3 || !receive_error;
    reset(); for (unsigned i=0; i<128; ++i) feed("X"); feed("\r\nGET_ANGLE\n");
    errors += queue_write!=1 || !receive_error || strcmp((const char *)commands[0],"GET_ANGLE")!=0;
#if IS_SRC
    {
        char echo[256];
        drain_echo(echo, sizeof echo);
        reset(); feed("\b\x7fGET_ANGLX\x7f\bLE\r"); drain_echo(echo, sizeof echo);
        errors += queue_write!=1 || strcmp((const char *)commands[0],"GET_ANGLE")!=0 || strcmp(echo,"GET_ANGLX\b \b\b \bLE\r\n")!=0;
        reset(); feed("GET_\x1b[A\x1b[3~ANGLE\r\n"); drain_echo(echo, sizeof echo);
        errors += queue_write!=1 || strcmp((const char *)commands[0],"GET_ANGLE")!=0 || strcmp(echo,"GET_ANGLE\r\n")!=0;
        reset(); feed("BAD\x03GET_ANGLE\r"); drain_echo(echo, sizeof echo);
        errors += queue_write!=1 || strcmp((const char *)commands[0],"GET_ANGLE")!=0 || strcmp(echo,"BAD^C\r\nGET_ANGLE\r\n")!=0;
        reset(); port.SR = USART_SR_FE | USART_SR_RXNE; port.DR = 0; USART1_IRQHandler();
        feed("GET_ANGLE\r"); drain_echo(echo, sizeof echo);
        errors += queue_write!=1 || !receive_error || strcmp((const char *)commands[0],"GET_ANGLE")!=0 || strcmp(echo,"GET_ANGLE\r\n")!=0;
        reset(); feed("GET_"); port.SR = USART_SR_FE | USART_SR_RXNE; port.DR = 0; USART1_IRQHandler();
        feed("ANGLE\rINFO_REQUEST\r");
        errors += queue_write!=1 || !receive_error || strcmp((const char *)commands[0],"INFO_REQUEST")!=0;
    }
#else
    (void)drain_echo;
#endif
    printf("Boundary failures: %u\n",errors);
    return errors ? 1 : 0;
}
'''.replace('__IS_SRC__', '0').replace('PREDICATE', predicate).replace('ISR', isr)
temporary = tempfile.TemporaryDirectory(prefix='btl-boundary-')
work = Path(temporary.name)
path = work / f'btl-boundary-{version}.c'
binary = work / f'btl-boundary-{version}'
path.write_text(source)
subprocess.run(['cc', '-std=c99', '-O2', '-Wall', '-Wextra', str(path), '-o', str(binary)], check=True)
result = subprocess.run([str(binary)]).returncode
temporary.cleanup()
sys.exit(result)
