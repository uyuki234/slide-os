#pragma once
#include <stdint.h>

#define WIN_MAX   6
#define WIN_W_MAX 78
#define WIN_H_MAX 23

typedef struct Window {
    int x, y, w, h;              /* 位置とサイズ(枠込み、単位は文字セル) */
    const char *title;
    uint16_t content[WIN_W_MAX * WIN_H_MAX]; /* 窓の中身(mallocがないので固定長) */
    int crow, ccol;              /* 窓内カーソル */
    uint8_t color;
    int used;
} Window;

Window *win_create(int x, int y, int w, int h, const char *title);
void win_putc(Window *w, char c);
void win_puts(Window *w, const char *s);
void win_clear(Window *w);
Window *wm_focused(void);
void wm_focus(Window *w);        /* 指定した窓を最前面に */
void wm_focus_next(void);
void wm_move_focused(int dx, int dy);
void wm_compose(void);           /* 全画面を合成してVGAに転送 */
