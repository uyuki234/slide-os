#include "kprintf.h"

#include <stdarg.h> /* ← libcではなくコンパイラ付属。freestandingでも使える */
#include <stdint.h>

static volatile uint16_t* const VGA = (uint16_t*)0xB8000;
enum { COLS = 80,
       ROWS = 25 };

/* コンソールの状態: カーソル位置と色。行0はステータスバー用に不可侵 */
static int row = 1, col = 0;
static uint8_t color = 0x0F;

void kset_color(uint8_t c) { color = c; }

static void scroll(void)
{
    /* 行2〜24を1行ずつ上へコピーし、最下行を空白で埋める */
    for (int r = 2; r < ROWS; r++)
        for (int c = 0; c < COLS; c++)
            VGA[(r - 1) * COLS + c] = VGA[r * COLS + c];
    for (int c = 0; c < COLS; c++)
        VGA[(ROWS - 1) * COLS + c] = (uint16_t)' ' | (color << 8);
}

void kputc(char ch)
{
    if (ch == '\n') {
        col = 0;
        row++;
    } else if (ch == '\b') {
        if (col > 0) VGA[row * COLS + --col] = (uint16_t)' ' | (color << 8);
    } else {
        VGA[row * COLS + col] = (uint8_t)ch | (color << 8);
        if (++col >= COLS) {
            col = 0;
            row++;
        } /* 右端で自動折り返し */
    }
    if (row >= ROWS) {
        scroll();
        row = ROWS - 1;
    }
}

static void kputs(const char* s)
{
    while (*s) kputc(*s++);
}

/* 汎用の数値出力: 任意の基数、符号対応 */
static void kput_num(uint32_t v, int base, int negative)
{
    char tmp[33];
    int i = 0;
    const char* digits = "0123456789abcdef";
    if (negative) kputc('-');
    do {
        tmp[i++] = digits[v % base];
        v /= base;
    } while (v);
    while (i--) kputc(tmp[i]); /* 下の桁から作ったので逆順に吐く */
}

void kprintf(const char* fmt, ...)
{
    va_list ap;
    va_start(ap, fmt); /* 「fmtの次の引数」から読み取り開始 */
    for (; *fmt; fmt++) {
        if (*fmt != '%') {
            kputc(*fmt);
            continue;
        }
        fmt++;
        if (*fmt == '\0') break; /* 「%」で終わる書式文字列で終端を踏み越えない */
        switch (*fmt) {
            case 'c':
                kputc((char)va_arg(ap, int));
                break;
            case 's':
                kputs(va_arg(ap, const char*));
                break;
            case 'u':
                kput_num(va_arg(ap, uint32_t), 10, 0);
                break;
            case 'x':
                kput_num(va_arg(ap, uint32_t), 16, 0);
                break;
            case 'd': {
                int32_t n = va_arg(ap, int32_t);
                if (n < 0)
                    /* -nだとINT32_MINで符号付きオーバーフロー(UB)。
                     * 符号なしに変換してから反転すれば全域で定義済み */
                    kput_num(-(uint32_t)n, 10, 1);
                else
                    kput_num((uint32_t)n, 10, 0);
                break;
            }
            case '%':
                kputc('%');
                break;
            default:
                kputc('%');
                kputc(*fmt); /* 未対応指定子はそのまま表示 */
        }
    }
    va_end(ap);
}
