#include "mouse.h"

#include <stdint.h>

#include "gfx.h"
#include "interrupts.h"
#include "io.h"
#include "kprintf.h"

/* ==== M4: PS/2マウス ====
 * キーボードと同じ8042コントローラの「第2ポート(AUX)」にぶら下がっている。
 * 有効化の儀式を済ませると、動かすたびに3バイト1組のパケットがIRQ12で届く。
 *   byte0: ボタン+符号+同期ビット(bit3が必ず1) / byte1: X移動量 / byte2: Y移動量 */

volatile int mouse_x = GFX_W / 2, mouse_y = GFX_H / 2;
volatile int mouse_btn;

static uint8_t pkt[3];
static int cycle; /* いま3バイト中の何バイト目か(パケットのステートマシン) */

/* 8042は遅いので「書ける/読める」まで待つ。無限ループ防止に回数上限付き */
static void wait_write(void)
{
    for (int i = 0; i < 100000; i++)
        if (!(inb(0x64) & 2)) return;
}
static void wait_read(void)
{
    for (int i = 0; i < 100000; i++)
        if (inb(0x64) & 1) return;
}

/* マウスへコマンドを送る: 0xD4を前置きすると8042がAUXへ転送してくれる */
static void mouse_write(uint8_t b)
{
    wait_write();
    outb(0x64, 0xD4);
    wait_write();
    outb(0x60, b);
    wait_read();
    (void)inb(0x60); /* 応答のACK(0xFA)を読み捨てて配管を詰まらせない */
}

static void on_mouse(struct regs *r)
{
    (void)r;
    if (!(inb(0x64) & 0x20)) { /* AUX由来でなければ(保険)読み捨てて帰る */
        (void)inb(0x60);
        return;
    }
    uint8_t b = inb(0x60);

    if (cycle == 0 && !(b & 0x08)) return; /* 同期ビット無し=ズレ→捨てて再同期 */
    pkt[cycle++] = b;
    if (cycle < 3) return;
    cycle = 0;

    if (pkt[0] & 0xC0) return; /* オーバーフロービット付きパケットは信用しない */
    /* 9bit符号付き: byte0のbit4/5が符号。負なら上位に0x100を借りて引く */
    int dx = (int)pkt[1] - (int)((pkt[0] << 4) & 0x100);
    int dy = (int)pkt[2] - (int)((pkt[0] << 3) & 0x100);
    mouse_x += dx;
    mouse_y -= dy; /* マウスは上がプラス、画面は下がプラスなので反転 */
    if (mouse_x < 0) mouse_x = 0;
    if (mouse_x > GFX_W - 1) mouse_x = GFX_W - 1;
    if (mouse_y < 0) mouse_y = 0;
    if (mouse_y > GFX_H - 1) mouse_y = GFX_H - 1;
    mouse_btn = pkt[0] & 7;
}

void mouse_init(void)
{
    wait_write();
    outb(0x64, 0xA8); /* AUXポートを有効化 */
    wait_write();
    outb(0x64, 0x20); /* 8042の設定バイトを読む */
    wait_read();
    uint8_t st = inb(0x60);
    st |= 0x02;  /* bit1: IRQ12を発生させる */
    st &= ~0x20; /* bit5: マウスのクロック停止を解除 */
    wait_write();
    outb(0x64, 0x60); /* 設定バイトを書き戻す */
    wait_write();
    outb(0x60, st);

    mouse_write(0xF6); /* 既定設定に戻す */
    mouse_write(0xF4); /* データ送信開始(これでパケットが流れ出す) */

    irq_install(44, on_mouse); /* 44 = 32+12 = IRQ12 */
    kprintf("[mouse] PS/2 aux enabled (IRQ12)\n");
}
