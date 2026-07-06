#include "serial.h"

#include <stdint.h>

#include "io.h"

/* ==== M0: シリアル出力(COM1) ====
 * グラフィックスモードに入ると0xB8000のテキスト画面は消える。
 * つまりkprintfが盲目になる。その前に「グラフィックス時代のprintf」を確保する。
 * QEMUを -serial stdio で起動すると、ここに書いた文字がターミナルに流れる */

enum { COM1 = 0x3F8 };

void serial_init(void)
{
    outb(COM1 + 1, 0x00); /* 割り込み無効(送信はポーリングで待つ) */
    outb(COM1 + 3, 0x80); /* DLAB=1: 次の2バイトはボーレート除数の意味になる */
    outb(COM1 + 0, 0x03); /* 除数下位 3 → 115200/3 = 38400bps */
    outb(COM1 + 1, 0x00); /* 除数上位 */
    outb(COM1 + 3, 0x03); /* DLAB戻し + 8bit・パリティなし・ストップ1 */
    outb(COM1 + 2, 0xC7); /* FIFO有効化・クリア */
    outb(COM1 + 4, 0x0B); /* DTR/RTS/OUT2 */
}

void serial_putc(char c)
{
    if (c == '\n') serial_putc('\r'); /* 端末の行頭復帰はCR+LFの作法 */
    /* LSR(ライン状態)のbit5 = 送信バッファ空き。空くまで待ってから書く */
    while (!(inb(COM1 + 5) & 0x20)) {}
    outb(COM1 + 0, (uint8_t)c);
}
