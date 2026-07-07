#pragma once
#include <stdint.h>

/* ==== M3: ウィンドウのピクセル移植 ====
 * 中身はv2と同じ「文字セルの格子」(cell = 文字 | 色番号<<8)のまま。
 * 描くときにビットマップフォントでピクセルへ展開する。
 * → v2のアプリ(win_putc/win_puts)が無改造で動く、が今回の答え合わせ */

#define WIN_MAX      6
#define WIN_COLS_MAX 64
#define WIN_ROWS_MAX 28
#define WIN_COLS_MIN 12
#define WIN_ROWS_MIN 3

enum { CELL_W = 8, CELL_H = 16, TITLE_H = 24, BORDER = 2 };

/* リサイズの「つかんだ場所」。右端・下端・右下角(両方) */
enum { RZ_NONE = 0, RZ_RIGHT = 1, RZ_BOTTOM = 2, RZ_BOTH = 3 };

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

/* 表示/非表示: 閉じてもWindowは死なない。zorderから抜けて描かれなくなるだけ。
 * 中身(cell)はそのまま残るので、タスクバーから復帰すると続きが見える */
void wm_hide(Window *w);
void wm_show(Window *w);     /* 非表示なら最前面に復帰、表示中なら前面化 */
int  wm_visible(Window *w);

int win_pw(Window *w); /* ピクセル幅(枠込み) */
int win_ph(Window *w); /* ピクセル高(枠込み) */

/* マウス用の当たり判定 */
Window *wm_hit(int px, int py);              /* 最前面から探す。なければ0 */
int wm_in_title(Window *w, int px, int py);  /* タイトルバー内か(ドラッグ開始判定) */
int wm_in_close(Window *w, int px, int py);  /* 赤丸(閉じるボタン)の上か */
Window *wm_taskbar_hit(int px, int py);      /* タスクバーのラベルを踏んだか */
int wm_resize_hit(int px, int py, Window **out); /* 端の8pxバンド。返り値=RZ_* */
void win_resize(Window *w, int cols, int rows);  /* セル単位。範囲は自動clamp */

void wm_set_status(const char *s); /* タスクバー右端(時計) */
void wm_compose(void);             /* 壁紙→窓→タスクバーを裏画面に合成 */
