#include "window.h"

static volatile uint16_t *const VGA = (uint16_t *)0xB8000;
enum { COLS = 80, ROWS = 25 };

static uint16_t backbuf[COLS * ROWS];   /* ★裏画面。描画は全部ここへ */

static Window pool[WIN_MAX];            /* mallocがないので静的プール */
static Window *zorder[WIN_MAX];         /* 描画順 = 重なり順。末尾が最前面 */
static int nwin;

static uint16_t cell(uint8_t ch, uint8_t color) {
    return (uint16_t)ch | ((uint16_t)color << 8);
}

Window *win_create(int x, int y, int w, int h, const char *title) {
    if (nwin >= WIN_MAX) return 0;
    Window *win = &pool[nwin];
    win->x = x; win->y = y; win->w = w; win->h = h;
    win->title = title; win->color = 0x0F;
    win->crow = 0; win->ccol = 0; win->used = 1;
    for (int i = 0; i < WIN_W_MAX * WIN_H_MAX; i++)
        win->content[i] = cell(' ', win->color);
    zorder[nwin++] = win;               /* 新しい窓は最前面に積む */
    return win;
}

/* --- 窓の中身への書き込み(kputcの窓内版: 折り返しとスクロール付き) --- */
static void win_scroll(Window *w) {
    int cw = w->w - 2, ch = w->h - 2;
    for (int r = 1; r < ch; r++)
        for (int c = 0; c < cw; c++)
            w->content[(r - 1) * WIN_W_MAX + c] = w->content[r * WIN_W_MAX + c];
    for (int c = 0; c < cw; c++)
        w->content[(ch - 1) * WIN_W_MAX + c] = cell(' ', w->color);
}

void win_putc(Window *w, char ch) {
    int cw = w->w - 2, chh = w->h - 2;
    if (ch == '\n') { w->ccol = 0; w->crow++; }
    else if (ch == '\b') {
        if (w->ccol > 0) w->content[w->crow * WIN_W_MAX + --w->ccol] = cell(' ', w->color);
    } else {
        w->content[w->crow * WIN_W_MAX + w->ccol] = cell((uint8_t)ch, w->color);
        if (++w->ccol >= cw) { w->ccol = 0; w->crow++; }
    }
    if (w->crow >= chh) { win_scroll(w); w->crow = chh - 1; }
}

void win_puts(Window *w, const char *s) { while (*s) win_putc(w, *s++); }

void win_clear(Window *w) {
    for (int i = 0; i < WIN_W_MAX * WIN_H_MAX; i++)
        w->content[i] = cell(' ', w->color);
    w->crow = 0; w->ccol = 0;
}

/* --- フォーカス管理 --- */
Window *wm_focused(void) { return nwin ? zorder[nwin - 1] : 0; }

void wm_focus_next(void) {              /* 最背面を最前面に回す=巡回 */
    if (nwin < 2) return;
    Window *bottom = zorder[0];
    for (int i = 0; i < nwin - 1; i++) zorder[i] = zorder[i + 1];
    zorder[nwin - 1] = bottom;
}

void wm_focus(Window *target) {
    for (int i = 0; i < nwin; i++) {
        if (zorder[i] != target) continue;
        for (int j = i; j < nwin - 1; j++) zorder[j] = zorder[j + 1];
        zorder[nwin - 1] = target;      /* 見つけた窓を末尾(最前面)へ回す */
        return;
    }
}

void wm_move_focused(int dx, int dy) {
    Window *w = wm_focused();
    if (w) { w->x += dx; w->y += dy; }
}

/* --- 合成: 壁紙→窓を奥から順に→一括転送 --- */
static void draw_window(Window *w, int focused) {
    uint8_t frame = focused ? 0x1F : 0x78;   /* 青地白 / 灰地黒 */
    for (int r = 0; r < w->h; r++) {
        int sy = w->y + r;
        if (sy < 0 || sy >= ROWS) continue;  /* ★クリッピング(縦) */
        for (int c = 0; c < w->w; c++) {
            int sx = w->x + c;
            if (sx < 0 || sx >= COLS) continue;  /* ★クリッピング(横) */
            uint16_t v;
            if (r == 0) {                    /* タイトルバー */
                const char *t = w->title;
                int len = 0; while (t[len]) len++;
                int tstart = (w->w - len) / 2;
                uint8_t ch = (c >= tstart && c < tstart + len)
                                 ? (uint8_t)t[c - tstart] : 0xCD;
                if (c == 0) ch = 0xC9;           /* ╔ */
                if (c == w->w - 1) ch = 0xBB;    /* ╗ */
                v = cell(ch, frame);
            } else if (r == w->h - 1) {      /* 下枠 */
                uint8_t ch = 0xCD;               /* ═ */
                if (c == 0) ch = 0xC8;           /* ╚ */
                if (c == w->w - 1) ch = 0xBC;    /* ╝ */
                v = cell(ch, frame);
            } else if (c == 0 || c == w->w - 1) {
                v = cell(0xBA, frame);           /* ║ 縦枠 */
            } else {                         /* 中身 */
                v = w->content[(r - 1) * WIN_W_MAX + (c - 1)];
            }
            backbuf[sy * COLS + sx] = v;
        }
    }
}

void wm_compose(void) {
    for (int i = 0; i < COLS * ROWS; i++)
        backbuf[i] = cell(0xB1, 0x03);      /* 壁紙: ▒をシアンで敷き詰め */
    for (int i = 0; i < nwin; i++)          /* ★奥から手前へ(画家のアルゴリズム) */
        draw_window(zorder[i], i == nwin - 1);
    for (int i = 0; i < COLS * ROWS; i++)   /* ★完成した1枚を一括転送 */
        VGA[i] = backbuf[i];
}
