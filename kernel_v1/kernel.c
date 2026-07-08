/* kernel.c --- slide-os本体: スライド表示 + キーボードでページ送り */
#include <stdint.h>

static volatile uint16_t* const VGA = (uint16_t*)0xB8000;
enum { COLS = 80,
       ROWS = 25 };

/* --- I/Oポート読み取り。キーボードはVGAと違い「ポート」経由で話す --- */
static inline uint8_t inb(uint16_t port)
{
    uint8_t v;
    __asm__ volatile("inb %1, %0" : "=a"(v) : "Nd"(port));
    return v;
}

/* --- 画面まわり(Step 2と同じ) --- */
static uint16_t vga_entry(char c, uint8_t color)
{
    return (uint16_t)c | ((uint16_t)color << 8);
}
static void clear_screen(uint8_t color)
{
    for (int i = 0; i < COLS * ROWS; i++)
        VGA[i] = vga_entry(' ', color);
}
static void put_str(int row, int col, const char* s, uint8_t color)
{
    for (int i = 0; s[i] != '\0'; i++)
        VGA[row * COLS + col + i] = vga_entry(s[i], color);
}

/* 1バイトを "3A" のような16進2桁で表示する */
static void put_hex8(int row, int col, uint8_t v, uint8_t color)
{
    const char* hex = "0123456789ABCDEF";
    char buf[3] = {hex[v >> 4], hex[v & 0x0F], 0};
    put_str(row, col, buf, color);
}

/* --- スライドのデータ。ここは自由に書き換える --- */
#define MAX_LINES 12
typedef struct {
    const char* title;
    const char* lines[MAX_LINES]; /* 末尾は0(番兵)で終わらせる */
} Slide;

static const Slide slides[] = {
    {"slide-os",
     {"An OS whose only job is showing slides.",
      "",
      "N: next   P: prev", 0}},
    {"1. printf needs an OS",
     {"printf(\"Hello\") works because an OS exists.",
      "Here, there is no OS. No libc. No main().", 0}},
    {"2. What runs before main()?",
     {"power on -> BIOS -> loader -> _start -> kmain()",
      "I wrote _start myself: set up the stack, call C.", 0}},
    {"3. How text appears",
     {"Writing to memory 0xB8000 = drawing on screen.",
      "This slide is literally an array assignment.", 0}},
    {"4. What I learned",
     {"- a lot happens before main()",
      "- nothing exists unless I build it:",
      "    no printf, no malloc, no fonts",
      "- building a piece of an OS taught me",
      "    a piece of how real ones work", 0}},
};
enum { NUM_SLIDES = sizeof(slides) / sizeof(slides[0]) };

static void draw_slide(int idx)
{
    clear_screen(0x0F);
    put_str(1, 3, slides[idx].title, 0x0B);
    for (int i = 0; slides[idx].lines[i] != 0; i++)
        put_str(4 + i, 5, slides[idx].lines[i], 0x0F);
    /* 右下にページ番号 "n/N"(9枚まで対応の手抜き版) */
    char page[4] = {(char)('1' + idx), '/', (char)('0' + NUM_SLIDES), 0};
    put_str(ROWS - 2, COLS - 6, page, 0x08);
}

void kmain(void)
{
    int idx = 0;
    draw_slide(idx);

    for (;;) {
        if (inb(0x64) & 1) {        /* 0x64: 状態レジスタ。bit0=データあり */
            uint8_t sc = inb(0x60); /* 0x60: スキャンコードを取り出す */
            put_str(ROWS - 2, 3, "scancode:", 0x08);
            put_hex8(ROWS - 2, 13, sc, 0x0E);                          /* 黄色で表示 */
            if (sc == 0x31 && idx < NUM_SLIDES - 1) draw_slide(++idx); /* N */
            if (sc == 0x19 && idx > 0) draw_slide(--idx);              /* P */
        }
        __asm__ volatile("pause"); /* 忙しい待ちループのCPUヒント */
    }
}
