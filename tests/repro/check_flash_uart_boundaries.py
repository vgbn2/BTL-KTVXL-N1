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
uart = (base / ('lib/shared/uart_cmd.c' if version == 'src' else 'uart_cmd.c')).read_text()
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
#define __DMB() ((void)0)
static struct { uint32_t SR, DR; } port;
#define UART_PORT (&port)
static volatile char commands[QUEUE_SIZE][RX_BUFFER_SIZE];
static volatile uint8_t queue_write, queue_read, receive_error;
static uint16_t rx_index;
static uint8_t discard_line;
static int accepts(unsigned manufacturer, unsigned type, unsigned capacity, int spi_error) {
PREDICATE
return 0;
}
ISR
static void reset(void) {
    queue_write = queue_read = receive_error = rx_index = discard_line = 0;
    memset((void *)commands, 0, sizeof commands);
}
static void feed(const char *s) {
    while (*s) { port.SR = USART_SR_RXNE; port.DR = (unsigned char)*s++; USART1_IRQHandler(); }
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
    printf("Boundary failures: %u\n",errors);
    return errors ? 1 : 0;
}
'''.replace('PREDICATE', predicate).replace('ISR', isr)
temporary = tempfile.TemporaryDirectory(prefix='btl-boundary-')
work = Path(temporary.name)
path = work / f'btl-boundary-{version}.c'
binary = work / f'btl-boundary-{version}'
path.write_text(source)
subprocess.run(['cc', '-std=c99', '-O2', '-Wall', '-Wextra', str(path), '-o', str(binary)], check=True)
result = subprocess.run([str(binary)]).returncode
temporary.cleanup()
sys.exit(result)
