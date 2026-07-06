#include "keyboard.h"

#include <stdint.h>

#include "interrupts.h"
#include "io.h"

/* ===== スキャンコード→ASCII変換表(USレイアウト、セット1) =====
 * 添字=スキャンコード、値=文字。0は「文字にならないキー」 */
static const char map_lo[128] =
    "\0\x1B"
    "1234567890-="
    "\b" /* 0x00-0x0E: Esc,数字列,Backspace */
    "\t"
    "qwertyuiop[]"
    "\n" /* 0x0F-0x1C: Tab,上段,Enter */
    "\0"
    "asdfghjkl;'`" /* 0x1D-0x29: Ctrl,中段 */
    "\0"
    "\\zxcvbnm,./"
    "\0"                        /* 0x2A-0x36: Shift,下段,Shift */
    "*\0 ";                     /* 0x37-0x39: *,Alt,Space */
static const char map_hi[128] = /* Shift押下時 */
    "\0\x1B"
    "!@#$%^&*()_+"
    "\b"
    "\t"
    "QWERTYUIOP{}"
    "\n"
    "\0"
    "ASDFGHJKL:\"~"
    "\0"
    "|ZXCVBNM<>?"
    "\0"
    "*\0 ";

/* ===== リングバッファ: 割り込みが書き、メインループが読む郵便受け =====
 * char → int16_t: 矢印キー等の特殊コード(0x100以上)も同じ郵便受けで運ぶ */
#define BUF_SIZE 128
static volatile int16_t buf[BUF_SIZE];
static volatile uint32_t head, tail; /* head=書き込み位置, tail=読み出し位置 */

static int shift;
static int e0; /* 「次は拡張キー」フラグ */

static void push(int16_t v) /* バッファ投入を関数に切り出し */
{
    uint32_t next = (head + 1) % BUF_SIZE; /* %で末尾から先頭へ輪になる */
    if (next != tail) {                    /* 満杯でなければ */
        buf[head] = v;
        head = next;
    } /* 満杯なら黙って捨てる(打鍵が速すぎた) */
}

static void on_keyboard(struct regs* r)
{
    (void)r;
    uint8_t sc = inb(0x60);

    if (sc == 0xE0) { /* 拡張キーの前触れ。v1実験で見たE0 */
        e0 = 1;
        return;
    }
    if (e0) {
        e0 = 0;
        if (sc & 0x80) return;
        switch (sc) { /* 2バイト目で矢印を判別 */
            case 0x48: push(KEY_UP); return;
            case 0x50: push(KEY_DOWN); return;
            case 0x4B: push(KEY_LEFT); return;
            case 0x4D: push(KEY_RIGHT); return;
        }
        return;
    }

    if (sc == 0x2A || sc == 0x36) {
        shift = 1;
        return;
    } /* Shift押した */
    if (sc == 0xAA || sc == 0xB6) {
        shift = 0;
        return;
    } /* Shift離した */
    if (sc & 0x80) return; /* その他のブレイクコード(離す)は無視 */

    char c = shift ? map_hi[sc] : map_lo[sc];
    if (c == 0) return; /* 文字にならないキー(F1やCtrl等) */
    push(c);
}

int kbd_getchar(void)
{
    if (tail == head) return -1; /* 空 */
    int v = buf[tail]; /* int16_tのまま返す: 0x100以上の特殊キーを潰さない */
    tail = (tail + 1) % BUF_SIZE;
    return v;
}

void keyboard_init(void)
{
    irq_install(33, on_keyboard);
}
