#include <stdint.h>

#include "io.h"
#include "interrupts.h"
#include "timer.h"
#include "keyboard.h"
#include "window.h"

static volatile uint16_t *const VGA = (uint16_t *)0xB8000;
enum { COLS = 80, ROWS = 25 };

static uint16_t vga_entry(char c, uint8_t color) {
    return (uint16_t)c | ((uint16_t)color << 8);
}
static void put_str(int row, int col, const char *s, uint8_t color) {
    for (int i = 0; s[i] != '\0'; i++)
        VGA[row * COLS + col + i] = vga_entry(s[i], color);
}
static void put_udec(int row, int col, uint32_t v, uint8_t color) {
    char buf[11]; int i = 10; buf[i] = 0;
    do { buf[--i] = '0' + v % 10; v /= 10; } while (v);
    put_str(row, col, &buf[i], color);
}
void kpanic(uint32_t int_no) {
    const char *hex = "0123456789ABCDEF";
    char b[3] = { hex[int_no >> 4], hex[int_no & 15], 0 };
    put_str(0, 0, "PANIC: exception", 0x4F);
    put_str(0, 17, b, 0x4F);
    for (;;) __asm__ volatile("cli; hlt");
}

/* ===== libc代替の小物 ===== */
static int streq(const char *a, const char *b) {
    while (*a && *a == *b) { a++; b++; }
    return *a == *b;
}
static void win_put_udec(Window *w, uint32_t v) {
    char buf[11]; int i = 10; buf[i] = 0;
    do { buf[--i] = '0' + v % 10; v /= 10; } while (v);
    win_puts(w, &buf[i]);
}

/* ===== アプリの枠組み: 窓 + キー担当関数 ===== */
typedef struct App App;
struct App {
    Window *win;
    void (*on_key)(App *, int key);   /* ★このアプリのキー処理係 */
};
static App home_app, slides_app, term_app;
static App *apps[] = { &home_app, &slides_app, &term_app };

static App *focused_app(void) {       /* 最前面の窓の持ち主を探す */
    Window *w = wm_focused();
    for (unsigned i = 0; i < sizeof(apps)/sizeof(apps[0]); i++)
        if (apps[i]->win == w) return apps[i];
    return 0;
}

/* ===== アプリ1: Slides(v1の魂の復活) ===== */
typedef struct { const char *title; const char *lines[10]; } Slide;
static const Slide slides[] = {
    { "slide-os", { "An OS whose only job is showing slides.",
        "", "N: next   P: prev", 0 } },
    { "1. printf needs an OS", { "printf(\"Hello\") works because an OS exists.",
        "Here, there is no OS. No libc. No main().", 0 } },
    { "2. What runs before main()?", { "power on -> BIOS -> loader -> _start -> kmain()",
        "I wrote _start myself: set up the stack, call C.", 0 } },
    { "3. How text appears", { "Writing to memory 0xB8000 = drawing on screen.",
        "This slide is literally an array assignment.", 0 } },
    { "4. What I learned", { "- a lot happens before main()",
        "- nothing exists unless I build it:",
        "    no printf, no malloc, no fonts",
        "- building a piece of an OS taught me",
        "    a piece of how real ones work", 0 } },
};
enum { NSLIDES = sizeof(slides)/sizeof(slides[0]) };
static int sidx;

static void slides_draw(void) {
    Window *w = slides_app.win;
    win_clear(w);
    win_puts(w, "  ");
    win_puts(w, slides[sidx].title);
    win_puts(w, "\n\n");
    for (int i = 0; slides[sidx].lines[i]; i++) {
        win_puts(w, "  ");
        win_puts(w, slides[sidx].lines[i]);
        win_putc(w, '\n');
    }
    win_puts(w, "\n  [");
    win_put_udec(w, sidx + 1);
    win_putc(w, '/');
    win_put_udec(w, NSLIDES);
    win_puts(w, "]");
}
static void slides_on_key(App *a, int k) {
    (void)a;
    if ((k == 'n' || k == 'N') && sidx < NSLIDES - 1) { sidx++; slides_draw(); }
    if ((k == 'p' || k == 'P') && sidx > 0)           { sidx--; slides_draw(); }
}

/* ===== アプリ2: ターミナル ===== */
static char line[64];
static int  llen;

static void term_prompt(void) { win_puts(term_app.win, "> "); }

static void term_exec(void) {
    Window *w = term_app.win;
    line[llen] = 0;                        /* 番兵を打って文字列として完成 */
    if (llen == 0) { }
    else if (streq(line, "help"))
        win_puts(w, "help about uptime clear\n");
    else if (streq(line, "about"))
        win_puts(w, "slide-os v2 (i386, multiboot)\nby uyuki + Fable, 2026-07-06\n");
    else if (streq(line, "uptime")) {
        uint32_t s = timer_ticks() / 100;
        win_put_udec(w, s / 60); win_puts(w, " min ");
        win_put_udec(w, s % 60); win_puts(w, " sec\n");
    }
    else if (streq(line, "clear")) win_clear(w);
    else { win_puts(w, "unknown: "); win_puts(w, line); win_putc(w, '\n'); }
    llen = 0;
    term_prompt();
}
static void term_on_key(App *a, int k) {
    Window *w = a->win;
    if (k == '\n') { win_putc(w, '\n'); term_exec(); }
    else if (k == '\b') { if (llen > 0) { llen--; win_putc(w, '\b'); } }
    else if (k >= ' ' && k < 0x7F && llen < 63) {
        line[llen++] = (char)k;
        win_putc(w, (char)k);              /* エコーは自前(本物のシェルと同じ) */
    }
}

/* ===== アプリ3: ホーム ===== */
static void home_draw(void) {
    Window *w = home_app.win;
    win_clear(w);
    win_puts(w, "  welcome to slide-os v2\n\n"
                "  1 : slides\n"
                "  2 : terminal\n\n"
                "  Tab: cycle  Esc: home\n"
                "  arrows: move window");
}
static void home_on_key(App *a, int k) {
    (void)a;
    if (k == '1') wm_focus(slides_app.win);
    if (k == '2') wm_focus(term_app.win);
}

/* ===== タスクバー(合成後に最下行へ直描き) ===== */
static void taskbar(void) {
    for (int i = 0; i < COLS; i++)
        VGA[(ROWS - 1) * COLS + i] = vga_entry(' ', 0x70);
    put_str(ROWS - 1, 1, "[ home | slides | terminal ]", 0x70);
    uint32_t sec = timer_ticks() / 100;
    put_str (ROWS - 1, COLS - 10, "up", 0x70);
    put_udec(ROWS - 1, COLS - 7, sec / 60, 0x70);
    put_str (ROWS - 1, COLS - 5, ":", 0x70);
    put_udec(ROWS - 1, COLS - 4, (sec % 60) / 10, 0x70);
    put_udec(ROWS - 1, COLS - 3, (sec % 60) % 10, 0x70);
}

void kmain(void) {
    gdt_init(); idt_init(); pic_remap();
    pic_set_masks(0xFC, 0xFF);
    timer_init(100);
    keyboard_init();

    slides_app.win = win_create(2, 2, 54, 16, " slides ");
    slides_app.on_key = slides_on_key;
    term_app.win = win_create(32, 8, 44, 14, " terminal ");
    term_app.on_key = term_on_key;
    home_app.win = win_create(48, 1, 30, 10, " home ");   /* 最後=起動時最前面 */
    home_app.on_key = home_on_key;

    slides_draw(); term_prompt(); home_draw();
    __asm__ volatile("sti");

    for (;;) {
        __asm__ volatile("hlt");
        int c;
        while ((c = kbd_getchar()) != -1) {
            if (c == '\t')           wm_focus_next();
            else if (c == 0x1B)      wm_focus(home_app.win);   /* Esc */
            else if (c == KEY_UP)    wm_move_focused(0, -1);
            else if (c == KEY_DOWN)  wm_move_focused(0,  1);
            else if (c == KEY_LEFT)  wm_move_focused(-1, 0);
            else if (c == KEY_RIGHT) wm_move_focused( 1, 0);
            else {                                  /* ★配達: 最前面のアプリへ */
                App *a = focused_app();
                if (a) a->on_key(a, c);
            }
        }
        wm_compose();
        taskbar();
    }
}
