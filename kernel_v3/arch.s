; arch.s --- Cでは書けない3つ: GDT切替、IDTロード、割り込み入口
BITS 32

; --- gdt_flush(ptr): 新しいGDTを有効化 ---
global gdt_flush
gdt_flush:
    mov eax, [esp+4]
    lgdt [eax]          ; GDTの場所をCPUに登録
    mov ax, 0x10        ; 0x10 = GDT3番目(データセグメント)
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    jmp 0x08:.flush     ; CSはmovできない。far jumpだけが書き換え手段
.flush:
    ret

global idt_load
idt_load:
    mov eax, [esp+4]
    lidt [eax]          ; IDTの場所をCPUに登録
    ret

; --- 割り込みスタブ48本(CPU例外0-31 + ハードウェアIRQ 32-47) ---
; CPUはエラーコードを積む例外と積まない例外があるので、
; 積まない方にはダミーの0を積んで、スタックの形を統一する
extern isr_dispatch

%macro ISR_NOERR 1
global isr%1
isr%1:
    push dword 0        ; ダミーエラーコード
    push dword %1       ; 自分の番号
    jmp isr_common
%endmacro

%macro ISR_ERR 1
global isr%1
isr%1:                  ; エラーコードはCPUが積み済み
    push dword %1
    jmp isr_common
%endmacro

ISR_NOERR 0
ISR_NOERR 1
ISR_NOERR 2
ISR_NOERR 3
ISR_NOERR 4
ISR_NOERR 5
ISR_NOERR 6
ISR_NOERR 7
ISR_ERR   8
ISR_NOERR 9
ISR_ERR   10
ISR_ERR   11
ISR_ERR   12
ISR_ERR   13
ISR_ERR   14
ISR_NOERR 15
ISR_NOERR 16
ISR_ERR   17
%assign i 18
%rep 14                 ; 18〜31(残りのCPU例外)
ISR_NOERR i
%assign i i+1
%endrep
%assign i 32
%rep 16                 ; 32〜47(IRQ0〜15)
ISR_NOERR i
%assign i i+1
%endrep

isr_common:
    pusha               ; 汎用レジスタ8本を全部退避
    push esp            ; いまのスタック先頭 = struct regsの先頭アドレス
    call isr_dispatch   ; Cの世界へ
    add esp, 4
    popa                ; 全レジスタ復元
    add esp, 8          ; int_noとエラーコードを捨てる
    iretd               ; 割り込み専用のreturn。eip/cs/eflagsを復元

; --- スタブのアドレス一覧表。C側がIDTに登録するのに使う ---
section .rodata
global isr_stub_table
isr_stub_table:
%assign i 0
%rep 48
    dd isr %+ i         ; %+ は文字列連結。isr0, isr1, ... と展開される
%assign i i+1
%endrep
