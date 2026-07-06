#include "interrupts.h"

#include "io.h"

/* ========== GDT: セグメントの定義表 ==========
 * QEMUのMultibootは「GDTは信用するな、自分で作れ」という仕様なので、
 * 割り込みを使う前に、確実に自分のものを用意する。
 * 中身は「メモリ全域を素通しで使う」だけの最小構成(フラットモデル) */
struct gdt_entry {
    uint16_t limit_lo, base_lo;
    uint8_t base_mid, access, gran, base_hi;
} __attribute__((packed)); /* packed: 構造体の隙間詰め禁止。
                              CPUが読む表なので1バイトもズレられない */
struct table_ptr {
    uint16_t limit;
    uint32_t base;
} __attribute__((packed));

static struct gdt_entry gdt[3];
static struct table_ptr gdtp;
extern void gdt_flush(struct table_ptr*);
extern void idt_load(struct table_ptr*);

static void gdt_set(int n, uint32_t base, uint32_t limit,
                    uint8_t access, uint8_t gran)
{
    gdt[n].base_lo = base & 0xFFFF;
    gdt[n].base_mid = (base >> 16) & 0xFF;
    gdt[n].base_hi = (base >> 24) & 0xFF;
    gdt[n].limit_lo = limit & 0xFFFF;
    gdt[n].gran = ((limit >> 16) & 0x0F) | (gran & 0xF0);
    gdt[n].access = access;
}

void gdt_init(void)
{
    gdt_set(0, 0, 0, 0, 0);             /* 0番: 必ず空、という決まり */
    gdt_set(1, 0, 0xFFFFF, 0x9A, 0xCF); /* 1番(0x08): コード用、全域 */
    gdt_set(2, 0, 0xFFFFF, 0x92, 0xCF); /* 2番(0x10): データ用、全域 */
    gdtp.limit = sizeof(gdt) - 1;
    gdtp.base = (uint32_t)&gdt;
    gdt_flush(&gdtp);
}

/* ========== IDT: 「事件番号→駆けつける関数」の対応表256件 ========== */
struct idt_entry {
    uint16_t off_lo, sel;
    uint8_t zero, flags;
    uint16_t off_hi;
} __attribute__((packed));

static struct idt_entry idt[256];
static struct table_ptr idtp;
extern uint32_t isr_stub_table[48]; /* arch.sで作った一覧表 */

static void idt_set(int n, uint32_t handler)
{
    idt[n].off_lo = handler & 0xFFFF;
    idt[n].sel = 0x08; /* ハンドラはコードセグメント0x08で動く */
    idt[n].zero = 0;
    idt[n].flags = 0x8E; /* 「有効な32bit割り込みゲート」の型番 */
    idt[n].off_hi = handler >> 16;
}

void idt_init(void)
{
    for (int i = 0; i < 48; i++)
        idt_set(i, isr_stub_table[i]);
    idtp.limit = sizeof(idt) - 1;
    idtp.base = (uint32_t)&idt;
    idt_load(&idtp);
}

/* ========== PIC: 割り込みの交通整理チップ ==========
 * 問題: 電源投入時、IRQ0〜7は「ベクタ8〜15」に届く設定になっているが、
 * そこはCPU例外(ダブルフォルト等)の指定席で、衝突する。
 * 解決: 初期化しなおして、IRQを32〜47に引っ越しさせる(remap) */
#define PIC1_CMD 0x20
#define PIC1_DAT 0x21
#define PIC2_CMD 0xA0
#define PIC2_DAT 0xA1

void pic_remap(void)
{
    outb(PIC1_CMD, 0x11); /* 初期化開始(カスケード構成・設定4語モード) */
    outb(PIC2_CMD, 0x11);
    outb(PIC1_DAT, 32); /* 親PIC: IRQ0-7 をベクタ32〜に */
    outb(PIC2_DAT, 40); /* 子PIC: IRQ8-15をベクタ40〜に */
    outb(PIC1_DAT, 4);  /* 親の2番ピンに子がぶら下がっている、という配線情報 */
    outb(PIC2_DAT, 2);
    outb(PIC1_DAT, 1); /* 8086モード */
    outb(PIC2_DAT, 1);
}

void pic_set_masks(uint8_t master, uint8_t slave)
{
    outb(PIC1_DAT, master); /* bitが1のIRQは無視される */
    outb(PIC2_DAT, slave);
}

static void pic_eoi(int irq)
{
    if (irq >= 8) outb(PIC2_CMD, 0x20);
    outb(PIC1_CMD, 0x20); /* 「処理完了」の報告。忘れると次が来ない */
}

/* ========== 振り分け係: 全割り込みがここに集まる ========== */
static void (*handlers[48])(struct regs*);

void irq_install(int vec, void (*handler)(struct regs*))
{
    handlers[vec] = handler;
}

extern void kpanic(uint32_t int_no); /* kernel.c側で画面表示 */

void isr_dispatch(struct regs* r)
{
    if (r->int_no < 48 && handlers[r->int_no])
        handlers[r->int_no](r);
    else if (r->int_no < 32)
        kpanic(r->int_no); /* 担当者不在のCPU例外 → 停止 */
    if (r->int_no >= 32)
        pic_eoi(r->int_no - 32);
}
