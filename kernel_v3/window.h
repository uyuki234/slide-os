#pragma once
#include <stdint.h>

/* ==== M3: ウィンドウのピクセル移植 ====
 * 中身はv2と同じ「文字セルの格子」(cell = 文字 | 色番号<<8)のまま。
 * 描くときにビットマップフォントでピクセルへ展開する。
 * → v2のアプリ(win_putc/win_puts)が無改造で動く、が今回の答え合わせ */

#define WIN_MAX      6
#define WIN_COLS_MAX 64
#define WIN_ROWS_MAX 28

enum { CELL_W = 8, CELL_H = 16, TITLE_H = 24, BORDER = 2 };

typedef struct Window {
    int x, y;       /* 画面上の左上(ピクセル、枠込み) */
    int cols, rows; /* 中身のテキスト格子サイズ */
    const char *title;
    uint16_t cell[WIN_COLS_MAX * WIN_ROWS_MAX];
    int crow, ccol; /* 窓内カーソル */
    uint8_t color;  /* パレット番号(v2と同じ0x00-0x0F) */
    int used;
} Window;

Window *win_create(int x, int y, int cols, int rows, const char *title);
void win_putc(Window *w, char c);
void win_puts(Window *w, const char *s);
void win_clear(Window *w);

Window *wm_focused(void);
void wm_focus(Window *w);
void wm_focus_next(void);
void wm_move_focused(int dx, int dy);

int win_pw(Window *w); /* ピクセル幅(枠込み) */
int win_ph(Window *w); /* ピクセル高(枠込み) */

/* マウス用の当たり判定 */
Window *wm_hit(int px, int py);              /* 最前面から探す。なければ0 */
int wm_in_title(Window *w, int px, int py);  /* タイトルバー内か(ドラッグ開始判定) */
Window *wm_taskbar_hit(int px, int py);      /* タスクバーのラベルを踏んだか */

void wm_set_status(const char *s); /* タスクバー右端(時計) */
void wm_compose(void);             /* 壁紙→窓→タスクバーを裏画面に合成 */
