#include "window.h"

#include "gfx.h"

/* ==== 色設計: Ubuntu風のアウベルジン(茄子紫)+オレンジ ==== */
enum {
    COL_WALL_TOP = 0x2C001E, /* 壁紙グラデーション上端(深い茄子色) */
    COL_WALL_BOT = 0x77216F, /* 下端(明るい紫) */
    COL_ACCENT   = 0xE95420, /* オレンジ(フォーカスの印) */
    COL_TITLE_A  = 0x4A2545, /* フォーカス窓タイトルバー上端 */
    COL_TITLE_B  = 0x2C1128, /* 同 下端 */
    COL_TITLE_NA = 0x2E2E36, /* 非フォーカスのタイトルバー */
    COL_TITLE_NB = 0x232329,
    COL_BODY     = 0x14141C, /* 窓の中身の背景 */
    COL_FRAME_N  = 0x3A3A44, /* 非フォーカスの枠 */
    COL_SHADOW   = 0x0E0410, /* 影(網点で半透明を偽装) */
    COL_BAR_A    = 0x1D1B22, /* タスクバー */
    COL_BAR_B    = 0x0F0E12,
    TASKBAR_H    = 32,
};

/* v2のVGA16色をRGBへ写像したパレット。cellの色番号で引く */
static const uint32_t pal[16] = {
    0x000000, 0x3465A4, 0x4E9A06, 0x06989A, 0xCC0000, 0x75507B, 0xC4A000,
    0xD3D7CF, 0x555753, 0x729FCF, 0x8AE234, 0x34E2E2, 0xEF2929, 0xAD7FA8,
    0xFCE94F, 0xFFFFFF,
};

static Window pool[WIN_MAX];
static Window *zorder[WIN_MAX];
static int nwin;

static char status[32];                 /* タスクバー右端の文字列 */
static uint32_t wall[GFX_W * GFX_H];    /* 壁紙キャッシュ(毎回描くと重いので) */
static int wall_ready;

static uint16_t cell(uint8_t ch, uint8_t color)
{
    return (uint16_t)ch | ((uint16_t)color << 8);
}

Window *win_create(int x, int y, int cols, int rows, const char *title)
{
    if (nwin >= WIN_MAX || cols > WIN_COLS_MAX || rows > WIN_ROWS_MAX)
        return 0;
    Window *win = &pool[nwin];
    win->x = x; win->y = y;
    win->cols = cols; win->rows = rows;
    win->title = title; win->color = 0x0F;
    win->crow = 0; win->ccol = 0; win->used = 1;
    for (int i = 0; i < WIN_COLS_MAX * WIN_ROWS_MAX; i++)
        win->cell[i] = cell(' ', win->color);
    zorder[nwin++] = win; /* 新しい窓は最前面に積む */
    return win;
}

int win_pw(Window *w) { return w->cols * CELL_W + BORDER * 2; }
int win_ph(Window *w) { return TITLE_H + w->rows * CELL_H + BORDER; }

/* --- 窓の中身への書き込み: v2のwin_putcと同一ロジック --- */
static void win_scroll(Window *w)
{
    int cw = w->cols, ch = w->rows;
    for (int r = 1; r < ch; r++)
        for (int c = 0; c < cw; c++)
            w->cell[(r - 1) * WIN_COLS_MAX + c] = w->cell[r * WIN_COLS_MAX + c];
    for (int c = 0; c < cw; c++)
        w->cell[(ch - 1) * WIN_COLS_MAX + c] = cell(' ', w->color);
}

void win_putc(Window *w, char ch)
{
    if (ch == '\n') { w->ccol = 0; w->crow++; }
    else if (ch == '\b') {
        if (w->ccol > 0)
            w->cell[w->crow * WIN_COLS_MAX + --w->ccol] = cell(' ', w->color);
    } else {
        w->cell[w->crow * WIN_COLS_MAX + w->ccol] = cell((uint8_t)ch, w->color);
        if (++w->ccol >= w->cols) { w->ccol = 0; w->crow++; }
    }
    if (w->crow >= w->rows) { win_scroll(w); w->crow = w->rows - 1; }
}

void win_puts(Window *w, const char *s) { while (*s) win_putc(w, *s++); }

void win_clear(Window *w)
{
    for (int i = 0; i < WIN_COLS_MAX * WIN_ROWS_MAX; i++)
        w->cell[i] = cell(' ', w->color);
    w->crow = 0; w->ccol = 0;
}

/* --- フォーカス管理(v2そのまま) --- */
Window *wm_focused(void) { return nwin ? zorder[nwin - 1] : 0; }

void wm_focus_next(void)
{
    if (nwin < 2) return;
    Window *bottom = zorder[0];
    for (int i = 0; i < nwin - 1; i++) zorder[i] = zorder[i + 1];
    zorder[nwin - 1] = bottom;
}

void wm_focus(Window *target)
{
    for (int i = 0; i < nwin; i++) {
        if (zorder[i] != target) continue;
        for (int j = i; j < nwin - 1; j++) zorder[j] = zorder[j + 1];
        zorder[nwin - 1] = target;
        return;
    }
}

void wm_move_focused(int dx, int dy)
{
    Window *w = wm_focused();
    if (w) { w->x += dx; w->y += dy; } /* クリッピングは描画側の仕事(v2と同じ) */
}

/* --- マウスの当たり判定 --- */
Window *wm_hit(int px, int py)
{
    for (int i = nwin - 1; i >= 0; i--) { /* 手前の窓から順に調べる */
        Window *w = zorder[i];
        if (px >= w->x && px < w->x + win_pw(w) && py >= w->y &&
            py < w->y + win_ph(w))
            return w;
    }
    return 0;
}

int wm_in_title(Window *w, int px, int py)
{
    return px >= w->x && px < w->x + win_pw(w) && py >= w->y &&
           py < w->y + TITLE_H;
}

/* 端の8pxバンド(+外側6pxのおまけ)に触れたらリサイズ。
 * 手前の窓から調べ、手前の窓の本体に当たったら奥の窓の端は隠れている扱い */
int wm_resize_hit(int px, int py, Window **out)
{
    for (int i = nwin - 1; i >= 0; i--) {
        Window *w = zorder[i];
        int pw = win_pw(w), ph = win_ph(w);
        if (px < w->x || px >= w->x + pw + 6 || py < w->y ||
            py >= w->y + ph + 6)
            continue;
        int mode = 0;
        if (px >= w->x + pw - 14 && py >= w->y + ph - 14)
            mode = RZ_BOTH; /* 角は広めに取って掴みやすく */
        else if (px >= w->x + pw - 8)
            mode = RZ_RIGHT;
        else if (py >= w->y + ph - 8)
            mode = RZ_BOTTOM;
        if (mode) {
            *out = w;
            return mode;
        }
        if (px < w->x + pw && py < w->y + ph)
            return RZ_NONE; /* 窓の本体に当たった: リサイズではない */
    }
    return RZ_NONE;
}

/* セル単位のリサイズ。中身のcellはストライド固定(WIN_COLS_MAX)なので、
 * 縮めても文字は消えず「隠れる」だけ。広げれば戻ってくる。
 * 行の折り返し直し(リフロー)はしない — 本物の端末もリサイズ時の
 * 再描画はアプリに任せる(SIGWINCHの世界)ので、手抜きではなく伝統 */
void win_resize(Window *w, int cols, int rows)
{
    if (cols < WIN_COLS_MIN) cols = WIN_COLS_MIN;
    if (cols > WIN_COLS_MAX) cols = WIN_COLS_MAX;
    if (rows < WIN_ROWS_MIN) rows = WIN_ROWS_MIN;
    if (rows > WIN_ROWS_MAX) rows = WIN_ROWS_MAX;
    w->cols = cols;
    w->rows = rows;
    if (w->ccol >= cols) w->ccol = cols - 1; /* カーソルを窓内に連れ戻す */
    if (w->crow >= rows) w->crow = rows - 1;
}

void wm_set_status(const char *s)
{
    int i = 0;
    for (; s[i] && i < 31; i++) status[i] = s[i];
    status[i] = '\0';
}

/* --- 壁紙: グラデーション+透かしリング。一度だけ作ってキャッシュ --- */
static void wall_init(void)
{
    fill_vgrad(0, 0, GFX_W, GFX_H, COL_WALL_TOP, COL_WALL_BOT);
    ring_dither(GFX_W - 180, GFX_H - 220, 190, 130, 0x9B5A8F, 3); /* 透かし */
    fill_circle(GFX_W - 180, GFX_H - 220 - 160, 14, 0xA96A9D);
    fill_circle(GFX_W - 180 - 139, GFX_H - 220 + 80, 14, 0xA96A9D);
    fill_circle(GFX_W - 180 + 139, GFX_H - 220 + 80, 14, 0xA96A9D);
    copy32(wall, gfx_back, GFX_W * GFX_H); /* 完成品を保存 */
    wall_ready = 1;
}

/* --- 1枚の窓を裏画面に描く(枠・タイトルバー・影・中身) --- */
static int slen(const char *s)
{
    int n = 0;
    while (s[n]) n++;
    return n;
}

static void draw_window(Window *w, int focused)
{
    int pw = win_pw(w), ph = win_ph(w);

    /* 影: 右+下に6pxずらした網点(1つ飛ばしに打つと半透明に見える) */
    for (int y = w->y + 6; y < w->y + ph + 6; y++)
        for (int x = w->x + pw; x < w->x + pw + 6; x++)
            if ((x + y) & 1) px(x, y, COL_SHADOW);
    for (int y = w->y + ph; y < w->y + ph + 6; y++)
        for (int x = w->x + 6; x < w->x + pw + 6; x++)
            if ((x + y) & 1) px(x, y, COL_SHADOW);

    /* 枠(2px)。フォーカス窓はオレンジ、他は灰色 */
    uint32_t frame = focused ? COL_ACCENT : COL_FRAME_N;
    fill_rect(w->x - BORDER, w->y - BORDER, pw + BORDER * 2, ph + BORDER * 2,
              frame);

    /* タイトルバー(グラデーション)+タイトル文字+飾りの丸3つ */
    if (focused)
        fill_vgrad(w->x, w->y, pw, TITLE_H, COL_TITLE_A, COL_TITLE_B);
    else
        fill_vgrad(w->x, w->y, pw, TITLE_H, COL_TITLE_NA, COL_TITLE_NB);
    fill_circle(w->x + 12, w->y + TITLE_H / 2, 5,
                focused ? COL_ACCENT : 0x55555F);
    fill_circle(w->x + 28, w->y + TITLE_H / 2, 5, 0x55555F);
    fill_circle(w->x + 44, w->y + TITLE_H / 2, 5, 0x55555F);
    draw_str(w->x + (pw - slen(w->title) * 8) / 2, w->y + (TITLE_H - 16) / 2,
             w->title, focused ? 0xFFFFFF : 0x9A9AA2);

    /* 中身: 暗い下地に文字セルをフォント展開 */
    fill_rect(w->x, w->y + TITLE_H, pw, ph - TITLE_H, COL_BODY);
    for (int r = 0; r < w->rows; r++)
        for (int c = 0; c < w->cols; c++) {
            uint16_t v = w->cell[r * WIN_COLS_MAX + c];
            char ch = (char)(v & 0xFF);
            if (ch != ' ')
                draw_char(w->x + BORDER + c * CELL_W,
                          w->y + TITLE_H + r * CELL_H, ch, pal[(v >> 8) & 0x0F]);
        }

    /* 右下角のリサイズグリップ(斜線3本)。「ここを掴めます」の目印 */
    uint32_t gc = focused ? 0xF0A582 : 0x5A5A64;
    for (int k = 4; k <= 12; k += 4)
        for (int t = 0; t <= k; t++)
            px(w->x + pw - 2 - t, w->y + ph - 2 - (k - t), gc);
}

/* --- タスクバー: 下端のバー。窓ラベル(クリック可)+時計 --- */
static int label_x0[WIN_MAX], label_x1[WIN_MAX]; /* 当たり判定用に記録 */

static void draw_taskbar(void)
{
    int y0 = GFX_H - TASKBAR_H;
    fill_vgrad(0, y0, GFX_W, TASKBAR_H, COL_BAR_A, COL_BAR_B);
    fill_rect(0, y0, GFX_W, 2, COL_ACCENT); /* 上端のアクセント線 */

    fill_circle(18, y0 + TASKBAR_H / 2 + 1, 7, COL_ACCENT); /* ロゴ印 */
    draw_str(34, y0 + 9, "slide-os v3", 0xEDEDF0);

    int x = 150;
    for (int i = 0; i < nwin; i++) { /* ラベルはzorderでなく生成順で安定させる */
        Window *w = &pool[i];
        int len = slen(w->title);
        label_x0[i] = x;
        label_x1[i] = x + 16 + len * 8;
        int foc = (wm_focused() == w);
        if (foc) { /* フォーカス中のラベルは下線+明るく */
            fill_rect(label_x0[i], y0 + TASKBAR_H - 4, label_x1[i] - label_x0[i],
                      3, COL_ACCENT);
        }
        draw_str(x + 8, y0 + 9, w->title, foc ? 0xFFFFFF : 0x8F8F98);
        x = label_x1[i] + 10;
    }

    draw_str(GFX_W - slen(status) * 8 - 14, y0 + 9, status, 0xEDEDF0);
}

Window *wm_taskbar_hit(int px, int py)
{
    if (py < GFX_H - TASKBAR_H) return 0;
    for (int i = 0; i < nwin; i++)
        if (px >= label_x0[i] && px < label_x1[i]) return &pool[i];
    return 0;
}

/* --- 合成: 壁紙→窓(奥から)→タスクバー。カーソルは呼び手が最後に描く --- */
void wm_compose(void)
{
    if (!wall_ready) wall_init();
    copy32(gfx_back, wall, GFX_W * GFX_H); /* 壁紙はキャッシュから一括転写 */
    for (int i = 0; i < nwin; i++)
        draw_window(zorder[i], i == nwin - 1); /* 画家のアルゴリズム(v2と同じ) */
    draw_taskbar();
}
