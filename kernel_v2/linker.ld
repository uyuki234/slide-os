/* linker.ld --- カーネルをメモリのどこに置くかの指示書 */
ENTRY(_start)                /* 実行開始点はboot.sの_start */

SECTIONS
{
    . = 1M;                  /* カーネルは1MB番地から配置(Multibootの慣習) */

    .multiboot : { *(.multiboot) }  /* ヘッダを必ず先頭に(規格: 先頭8KB以内) */
    .text      : { *(.text) }       /* 機械語 */
    .rodata    : { *(.rodata) }     /* 文字列などの読み取り専用データ */
    .data      : { *(.data) }       /* 初期値ありの変数 */
    .bss       : { *(.bss) }        /* 初期値なしの変数、スタックもここ */
}
