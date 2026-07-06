; boot.s --- v3: Multibootヘッダにビデオモード希望を追加し、GRUBの返事(ebx)を回収する
BITS 32

MAGIC    equ 0x1BADB002      ; Multiboot規格の合言葉
; bit0: モジュール4KB整列 / bit1: メモリ情報希望 / bit2: ビデオモード希望 ←★v3の主役
FLAGS    equ 00000111b
CHECKSUM equ -(MAGIC + FLAGS)

section .multiboot
align 4
    dd MAGIC
    dd FLAGS
    dd CHECKSUM
    ; ここから下はflags bit2(ビデオ)を立てたときに読まれる追加フィールド
    dd 0, 0, 0, 0, 0         ; header/load/entryアドレス群(bit16未使用なので0で場所だけ)
    dd 0                     ; mode_type: 0 = リニアグラフィックス
    dd 1024                  ; 幅(希望)
    dd 768                   ; 高さ(希望)
    dd 32                    ; 1ピクセルのビット数(希望)

section .bss
align 16
stack_bottom: resb 16384
stack_top:

section .text
global _start
extern kmain
_start:
    mov esp, stack_top
    ; GRUBは eax にマジック、ebx にMultiboot info構造体の物理アドレスを入れて来る。
    ; v1/v2はこれを無視してきた。v3で初めて「ブートローダからの引き継ぎ書類」を受け取る
    push ebx
    push eax
    call kmain               ; kmain(uint32_t magic, uint32_t mbinfo)
.hang:
    cli
    hlt
    jmp .hang
