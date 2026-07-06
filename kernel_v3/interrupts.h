#pragma once
#include <stdint.h>

/* 割り込み発生時のレジスタ一式。arch.sが積んだスタックをCから見た形 */
struct regs {
    uint32_t edi, esi, ebp, esp_, ebx, edx, ecx, eax; /* pushaの分 */
    uint32_t int_no, err;                             /* スタブが積んだ分 */
    uint32_t eip, cs, eflags;                         /* CPUが自動で積んだ分 */
};

void gdt_init(void);
void idt_init(void);
void pic_remap(void);
void pic_set_masks(uint8_t master, uint8_t slave);
void irq_install(int vec, void (*handler)(struct regs *));
