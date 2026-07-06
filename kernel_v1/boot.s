; boot.s --- カーネルの入口。Multibootヘッダ + スタック準備 + Cへの橋渡し
BITS 32                      ; QEMUに渡された時点で、既に32bitプロテクトモード

MAGIC    equ 0x1BADB002      ; Multiboot規格の合言葉
FLAGS    equ 0
CHECKSUM equ -(MAGIC + FLAGS); 3つ足すと0になる、という検算用の値

section .multiboot           ; この12バイトをファイル先頭付近に置く(linker.ldで保証)
align 4
    dd MAGIC
    dd FLAGS
    dd CHECKSUM

section .bss
align 16
stack_bottom: resb 16384     ; 16KBのスタック領域を予約
stack_top:                   ; スタックは高い番地から低い方へ伸びるので「上端」を使う

section .text
global _start                ; リンカに「入口はここ」と教えるためのラベル公開
extern kmain                 ; kmainはC側にある、という宣言
_start:
    mov esp, stack_top       ; ★Cが動くための最重要準備: スタックポインタ設定
    call kmain               ; C言語の世界へ
.hang:                       ; 万一kmainから戻ってきたときの安全網
    cli
    hlt
    jmp .hang
