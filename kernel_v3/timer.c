#include "timer.h"

#include "interrupts.h"
#include "io.h"

/* OSの心臓の鼓動カウンタ。割り込みの中と外の両方から触るのでvolatile */
static volatile uint32_t ticks;

static void on_timer(struct regs* r)
{
    (void)r;
    ticks++; /* IRQ0が来るたびに+1。やることはこれだけ */
}

void timer_init(uint32_t hz)
{
    /* PITの入力クロックは1193182Hz(歴史的事情の半端な数)。
     * 「何回数えたら1回鳴らすか」の分周値を渡すと、好きな周期になる */
    uint32_t div = 1193182 / hz;
    outb(0x43, 0x36);       /* チャンネル0、周期モードで使う宣言 */
    outb(0x40, div & 0xFF); /* 分周値を下位→上位の順で送る */
    outb(0x40, (div >> 8) & 0xFF);
    irq_install(32, on_timer); /* 32 = IRQ0 */
}

uint32_t timer_ticks(void) { return ticks; }
