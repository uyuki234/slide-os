#include <stdint.h>

#include "gfx.h"
#include "interrupts.h"
#include "keyboard.h"
#include "kprintf.h"
#include "mouse.h"
#include "serial.h"
#include "timer.h"
#include "window.h"

/* CPU例外の最終防衛ライン: シリアルに叫び、画面上部に赤帯を出して停止 */
void kpanic(uint32_t int_no)
{
    kprintf("[PANIC] unhandled exception %u\n", int_no);
    fill_rect(0, 0, GFX_W, 44, 0xCC0000);
    draw_str(16, 14, "PANIC: unhandled exception", 0xFFFFFF);
    gfx_flush();
    for (;;) __asm__ volatile("cli; hlt");
}

/* ===== libc代替の小物(v2から) ===== */
static int streq(const char *a, const char *b)
{
    while (*a && *a == *b) { a++; b++; }
    return *a == *b;
}
static void win_put_udec(Window *w, uint32_t v)
{
    char buf[11];
    int i = 10;
    buf[i] = 0;
    do { buf[--i] = '0' + v % 10; v /= 10; } while (v);
    win_puts(w, &buf[i]);
}
static void fmt_uptime(char out[9], uint32_t sec)
{
    out[0] = 'u'; out[1] = 'p'; out[2] = ' ';
    out[3] = '0' + (sec / 600) % 10;
    out[4] = '0' + (sec / 60) % 10;
    out[5] = ':';
    out[6] = '0' + (sec % 60) / 10;
    out[7] = '0' + (sec % 60) % 10;
    out[8] = '\0';
}

/* ===== アプリの枠組み: 窓 + キー担当関数(v2の関数ポインタディスパッチ) ===== */
typedef struct App App;
struct App {
    Window *win;
    void (*on_key)(App *, int key);
};
static App home_app, slides_app, term_app;
static App *apps[] = { &home_app, &slides_app, &term_app };

static App *focused_app(void)
{
    Window *w = wm_focused();
    for (unsigned i = 0; i < sizeof(apps) / sizeof(apps[0]); i++)
        if (apps[i]->win == w) return apps[i];
    return 0;
}

/* ===== アプリ1: Slides ===== */
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
enum { NSLIDES = sizeof(slides) / sizeof(slides[0]) };
static int sidx;

static void slides_draw(void)
{
    Window *w = slides_app.win;
    win_clear(w);
    w->color = 0x0E; /* タイトルは黄色 */
    win_puts(w, "  ");
    win_puts(w, slides[sidx].title);
    win_puts(w, "\n\n");
    w->color = 0x0F;
    for (int i = 0; slides[sidx].lines[i]; i++) {
        win_puts(w, "  ");
        win_puts(w, slides[sidx].lines[i]);
        win_putc(w, '\n');
    }
    w->color = 0x08;
    win_puts(w, "\n  [");
    win_put_udec(w, sidx + 1);
    win_putc(w, '/');
    win_put_udec(w, NSLIDES);
    win_puts(w, "]");
    w->color = 0x0F;
}
static void slides_on_key(App *a, int k)
{
    (void)a;
    if ((k == 'n' || k == 'N') && sidx < NSLIDES - 1) { sidx++; slides_draw(); }
    if ((k == 'p' || k == 'P') && sidx > 0)           { sidx--; slides_draw(); }
}

/* ===== アプリ2: ターミナル ===== */
static char line[64];
static int llen;

static void term_prompt(void)
{
    Window *w = term_app.win;
    w->color = 0x0A;
    win_puts(w, "> ");
    w->color = 0x0F;
}

static void term_exec(void)
{
    Window *w = term_app.win;
    line[llen] = 0;
    if (llen == 0) { }
    else if (streq(line, "help"))
        win_puts(w, "help about uptime clear\n");
    else if (streq(line, "about"))
        win_puts(w, "slide-os v3 (i386, multiboot, GUI)\n"
                    "by uyuki + Fable, 2026-07-07\n");
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
static void term_on_key(App *a, int k)
{
    Window *w = a->win;
    if (k == '\n') { win_putc(w, '\n'); term_exec(); }
    else if (k == '\b') { if (llen > 0) { llen--; win_putc(w, '\b'); } }
    else if (k >= ' ' && k < 0x7F && llen < 63) {
        line[llen++] = (char)k;
        win_putc(w, (char)k);
    }
}

/* ===== アプリ3: ホーム ===== */
static void home_draw(void)
{
    Window *w = home_app.win;
    win_clear(w);
    win_puts(w, "  welcome to slide-os v3\n\n"
                "  1 : slides\n"
                "  2 : terminal\n\n"
                "  Tab : cycle   Esc : home\n"
                "  arrows : move window\n\n");
    w->color = 0x07;
    win_puts(w, "  mouse : click to focus,\n"
                "  drag title bar to move,\n"
                "  drag edge/corner to resize\n"
                "  red dot : close window\n"
                "  taskbar : reopen app");
    w->color = 0x0F;
}
static void home_on_key(App *a, int k)
{
    (void)a;
    if (k == '1') wm_show(slides_app.win); /* 閉じていても呼び出せる */
    if (k == '2') wm_show(term_app.win);
}

/* ===== 起動スプラッシュ: リングと再生ボタン(スライドを上映するOSの紋章) ===== */
static void splash_draw(void)
{
    int cx = GFX_W / 2, cy = 300;
    fill_vgrad(0, 0, GFX_W, GFX_H, 0x1E0014, 0x4A1042);
    fill_circle(cx + 5, cy + 7, 93, 0x160010);          /* エンブレムの影 */
    ring(cx, cy, 90, 66, 0xE95420);                     /* オレンジのリング */
    fill_tri(cx - 26, cy - 40, cx - 26, cy + 40, cx + 46, cy, 0xFFFFFF);
    draw_str_big(cx - text_w("slide-os", 6) / 2, 430, "slide-os", 0xFFFFFF, 6);
    draw_str_big(cx - text_w("v3", 3) / 2, 545, "v3", 0xE95420, 3);
    draw_str(cx - text_w("the OS whose job is showing slides", 1) / 2, 610,
             "the OS whose job is showing slides", 0xC49CB8);
}

static void splash_progress(uint32_t until_tick)
{
    int bw = 320, bx = (GFX_W - bw) / 2, by = 660;
    fill_rect(bx, by, bw, 8, 0x33102E); /* プログレスバーの溝 */
    gfx_flush();
    uint32_t t0 = timer_ticks();
    while (timer_ticks() < until_tick) {
        uint32_t t = timer_ticks();
        int fill = (int)((t - t0) * (uint32_t)bw / (until_tick - t0));
        if (fill > bw) fill = bw;
        fill_rect(bx, by, fill, 8, 0xE95420);
        gfx_flush_rect(bx, by, bw, 8); /* バーの帯だけ転写(全画面は無駄) */
        __asm__ volatile("hlt");
    }
}

/* ===== マウスカーソル: 矢印+リサイズ用3種。合成の一番最後に直接描く ===== */
static const char *cur_arrow[19] = {
    "X           ", "XX          ", "X.X         ", "X..X        ",
    "X...X       ", "X....X      ", "X.....X     ", "X......X    ",
    "X.......X   ", "X........X  ", "X.....XXXXX ", "X..X..X     ",
    "X.X X..X    ", "XX  X..X    ", "X    X..X   ", "     X..X   ",
    "      X..X  ", "      X..X  ", "       XX   ",
};
static const char *cur_h[9] = { /* ↔ 左右リサイズ */
    "    X      X    ",
    "   XX      XX   ",
    "  X.X      X.X  ",
    " X..XXXXXXXX..X ",
    "X..............X",
    " X..XXXXXXXX..X ",
    "  X.X      X.X  ",
    "   XX      XX   ",
    "    X      X    ",
};
static const char *cur_v[16] = { /* ↕ 上下リサイズ */
    "    X    ", "   X.X   ", "  X...X  ", " X.....X ",
    " XXX.XXX ", "   X.X   ", "   X.X   ", "   X.X   ",
    "   X.X   ", "   X.X   ", "   X.X   ", " XXX.XXX ",
    " X.....X ", "  X...X  ", "   X.X   ", "    X    ",
};
static const char *cur_d[12] = { /* ⤡ 斜めリサイズ(右下角) */
    "XXXXX       ", "X...X       ", "X..X        ", "X.X.X       ",
    "XX X.X      ", "    X.X     ", "     X.X    ", "      X.X XX",
    "       X.X.X", "        X..X", "       X...X", "       XXXXX",
};
static void draw_img(int x, int y, const char **img, int n)
{
    for (int r = 0; r < n; r++)
        for (int c = 0; img[r][c]; c++) {
            char ch = img[r][c];
            if (ch == 'X') px(x + c, y + r, 0x101014);
            else if (ch == '.') px(x + c, y + r, 0xFFFFFF);
        }
}
static void draw_cursor(int mx, int my, int rz_mode)
{
    if (rz_mode == RZ_RIGHT)       draw_img(mx - 8, my - 4, cur_h, 9);
    else if (rz_mode == RZ_BOTTOM) draw_img(mx - 4, my - 8, cur_v, 16);
    else if (rz_mode == RZ_BOTH)   draw_img(mx - 6, my - 6, cur_d, 12);
    else                           draw_img(mx, my, cur_arrow, 19);
}

void kmain(uint32_t magic, uint32_t mbinfo)
{
    serial_init();
    kprintf("\n[boot] slide-os v3 starting\n");

    /* M1の防御チェック: GRUB以外(-kernel直接)で起動されたらここで止まる */
    if (gfx_init(magic, mbinfo) != 0) {
        kprintf("[boot] no framebuffer -> halt. use `make run-iso`.\n");
        for (;;) __asm__ volatile("cli; hlt");
    }

    splash_draw();
    gfx_flush(); /* 割り込みの準備より先に、まずロゴを出す(体感起動速度) */
    kprintf("[boot] splash on screen\n");

    gdt_init();
    idt_init();
    pic_remap();
    pic_set_masks(0xF8, 0xEF); /* IRQ0,1,2(カスケード)+IRQ12(マウス)を通す */
    timer_init(100);
    keyboard_init();
    mouse_init();
    __asm__ volatile("sti");

    splash_progress(250); /* 2.5秒かけてプログレスバーを満たす */
    kprintf("[boot] entering desktop\n");

    /* デスクトップ組み立て。生成順=タスクバー順、最後に作った窓が最前面 */
    slides_app.win = win_create(96, 120, 52, 16, " slides ");
    slides_app.on_key = slides_on_key;
    term_app.win = win_create(420, 380, 46, 14, " terminal ");
    term_app.on_key = term_on_key;
    home_app.win = win_create(600, 96, 32, 13, " home ");
    home_app.on_key = home_on_key;
    slides_draw();
    term_prompt();
    home_draw();

    int dirty = 1;
    int pmx = mouse_x, pmy = mouse_y, pbtn = 0;
    Window *drag = 0;
    int dragox = 0, dragoy = 0;
    Window *rz_win = 0; /* リサイズ中の窓と、掴んだ端の種類 */
    int rz_mode = RZ_NONE, rz_ox = 0, rz_oy = 0;
    uint32_t shown_sec = (uint32_t)-1;

    for (;;) {
        __asm__ volatile("hlt");

        /* 時計: 秒が変わった時だけタスクバー更新(無駄な再描画はしない) */
        uint32_t sec = timer_ticks() / 100;
        if (sec != shown_sec) {
            shown_sec = sec;
            char st[9];
            fmt_uptime(st, sec);
            wm_set_status(st);
            dirty = 1;
        }

        /* キーボード: グローバルキーを横取りし、残りを最前面アプリへ配達 */
        int c;
        while ((c = kbd_getchar()) != -1) {
            if (c == '\t')           wm_focus_next();
            else if (c == 0x1B)      wm_show(home_app.win); /* Esc(閉じていても復帰) */
            else if (c == KEY_UP)    wm_move_focused(0, -16);
            else if (c == KEY_DOWN)  wm_move_focused(0, 16);
            else if (c == KEY_LEFT)  wm_move_focused(-16, 0);
            else if (c == KEY_RIGHT) wm_move_focused(16, 0);
            else {
                App *a = focused_app();
                if (a) a->on_key(a, c);
            }
            dirty = 1;
        }

        /* マウス: クリックでフォーカス、タイトルバーをつかむとドラッグ移動 */
        int mx = mouse_x, my = mouse_y, mb = mouse_btn;
        if (mx != pmx || my != pmy) dirty = 1;
        if ((mb & 1) && !(pbtn & 1)) { /* 左ボタンを押した瞬間だけ判定 */
            Window *rw = 0;
            int m = wm_resize_hit(mx, my, &rw); /* 端バンドが最優先 */
            Window *tw = m ? 0 : wm_taskbar_hit(mx, my);
            Window *hw = (m || tw) ? 0 : wm_hit(mx, my);
            kprintf("[mouse] press %d,%d -> %s\n", mx, my,
                    m ? "resize" : tw ? "taskbar" : hw ? hw->title : "desktop");
            if (m) { /* 端を掴んだ: 窓の右下端と掴んだ点のズレを覚えておく */
                wm_focus(rw);
                rz_win = rw;
                rz_mode = m;
                rz_ox = rw->x + win_pw(rw) - mx;
                rz_oy = rw->y + win_ph(rw) - my;
            } else if (tw) {
                kprintf("[mouse] show '%s'\n", tw->title);
                wm_show(tw); /* 閉じていれば復帰、出ていれば前面化 */
            } else if (hw) {
                if (wm_in_close(hw, mx, my)) { /* 赤丸=閉じる */
                    kprintf("[mouse] closed '%s'\n", hw->title);
                    wm_hide(hw);
                } else {
                    wm_focus(hw);
                    if (wm_in_title(hw, mx, my)) { /* タイトルバー=窓の取っ手 */
                        drag = hw;
                        dragox = mx - hw->x;
                        dragoy = my - hw->y;
                    }
                }
            }
            dirty = 1;
        }
        if (!(mb & 1)) {
            if (drag)
                kprintf("[mouse] dropped '%s' at %d,%d\n", drag->title,
                        drag->x, drag->y);
            if (rz_win)
                kprintf("[mouse] resized '%s' to %dx%d\n", rz_win->title,
                        rz_win->cols, rz_win->rows);
            drag = 0;
            rz_win = 0;
        }
        if (rz_win) { /* カーソル位置から目標ピクセル寸法→セル数へ丸める */
            int cols = rz_win->cols, rows = rz_win->rows;
            if (rz_mode & RZ_RIGHT)
                cols = (mx + rz_ox - rz_win->x - BORDER * 2 + CELL_W / 2) /
                       CELL_W;
            if (rz_mode & RZ_BOTTOM)
                rows = (my + rz_oy - rz_win->y - TITLE_H - BORDER +
                        CELL_H / 2) / CELL_H;
            win_resize(rz_win, cols, rows);
            dirty = 1;
        } else if (drag) {
            drag->x = mx - dragox;
            drag->y = my - dragoy;
            dirty = 1;
        }
        pmx = mx; pmy = my; pbtn = mb;

        if (dirty) { /* 変わった時だけ 合成→カーソル→一括転写 */
            int hover = rz_mode; /* リサイズ中はその形のカーソルを維持 */
            if (!rz_win) {
                Window *hv = 0;
                hover = drag ? RZ_NONE : wm_resize_hit(mx, my, &hv);
            }
            wm_compose();
            draw_cursor(mx, my, hover);
            gfx_flush();
            dirty = 0;
        }
    }
}
