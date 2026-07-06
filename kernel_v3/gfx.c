#include "gfx.h"

#include "font8x16.h"
#include "kprintf.h"

uint32_t gfx_back[GFX_W * GFX_H]; /* 裏画面 3MiB。BSSに置く(要件定義§3) */

static volatile uint8_t *fb; /* GRUBがくれた本物のフレームバッファ */
static uint32_t fb_pitch;    /* 1行のバイト数。width*4と一致するとは限らない! */

/* ==== M1: Multiboot info構造体からフレームバッファ情報を回収 ====
 * GRUBは応答としてflagsのbit12を立て、アドレス/pitch/解像度/bppを書いてくる。
 * QEMUの -kernel 直接起動はビデオ要求を無視するので、ここで検出して止まる */
int gfx_init(uint32_t magic, uint32_t mbinfo)
{
    if (magic != 0x2BADB002) {
        kprintf("[gfx] bad multiboot magic: 0x%x\n", magic);
        return -1;
    }
    const uint8_t *mb = (const uint8_t *)mbinfo;
    uint32_t flags = *(const uint32_t *)mb;
    if (!(flags & (1u << 12))) { /* framebuffer情報なし = GRUB経由でない */
        kprintf("[gfx] no framebuffer info (flags=0x%x). boot via GRUB ISO!\n",
                flags);
        return -1;
    }
    uint32_t addr = *(const uint32_t *)(mb + 88); /* 64bit値の下位32bit */
    fb_pitch = *(const uint32_t *)(mb + 96);
    uint32_t w = *(const uint32_t *)(mb + 100);
    uint32_t h = *(const uint32_t *)(mb + 104);
    uint8_t bpp = *(mb + 108);
    kprintf("[gfx] fb addr=0x%x pitch=%u %ux%u bpp=%u\n", addr, fb_pitch, w, h,
            (uint32_t)bpp);
    if (w != GFX_W || h != GFX_H || bpp != 32) {
        kprintf("[gfx] unexpected mode (want %ux%ux32)\n", GFX_W, GFX_H);
        return -1;
    }
    fb = (volatile uint8_t *)addr;
    return 0;
}

void px(int x, int y, uint32_t c)
{
    if ((unsigned)x < GFX_W && (unsigned)y < GFX_H)
        gfx_back[y * GFX_W + x] = c;
}

void fill_rect(int x, int y, int w, int h, uint32_t c)
{
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > GFX_W) w = GFX_W - x;
    if (y + h > GFX_H) h = GFX_H - y;
    for (int r = 0; r < h; r++) {
        uint32_t *p = &gfx_back[(y + r) * GFX_W + x];
        for (int i = 0; i < w; i++) p[i] = c;
    }
}

/* 縦グラデーション: 上端topから下端botへ1行ずつ色を補間して塗る */
void fill_vgrad(int x, int y, int w, int h, uint32_t top, uint32_t bot)
{
    int r0 = (top >> 16) & 0xFF, g0 = (top >> 8) & 0xFF, b0 = top & 0xFF;
    int r1 = (bot >> 16) & 0xFF, g1 = (bot >> 8) & 0xFF, b1 = bot & 0xFF;
    for (int i = 0; i < h; i++) {
        int rr = r0 + (r1 - r0) * i / (h > 1 ? h - 1 : 1);
        int gg = g0 + (g1 - g0) * i / (h > 1 ? h - 1 : 1);
        int bb = b0 + (b1 - b0) * i / (h > 1 ? h - 1 : 1);
        fill_rect(x, y + i, w, 1,
                  ((uint32_t)rr << 16) | ((uint32_t)gg << 8) | (uint32_t)bb);
    }
}

static int isqrt(int v) /* 整数平方根(円の縁の計算用) */
{
    int r = 0;
    while ((r + 1) * (r + 1) <= v) r++;
    return r;
}

void fill_circle(int cx, int cy, int r, uint32_t c)
{
    for (int dy = -r; dy <= r; dy++) {
        int dx = isqrt(r * r - dy * dy);
        fill_rect(cx - dx, cy + dy, dx * 2 + 1, 1, c);
    }
}

/* ドーナツ形。1行を「外円の幅」から「内円の幅」を抜いた左右2本で塗る */
void ring(int cx, int cy, int r_out, int r_in, uint32_t c)
{
    for (int dy = -r_out; dy <= r_out; dy++) {
        int xo = isqrt(r_out * r_out - dy * dy);
        int xi = (dy >= -r_in && dy <= r_in) ? isqrt(r_in * r_in - dy * dy) : -1;
        if (xi < 0) {
            fill_rect(cx - xo, cy + dy, xo * 2 + 1, 1, c);
        } else {
            fill_rect(cx - xo, cy + dy, xo - xi, 1, c);
            fill_rect(cx + xi + 1, cy + dy, xo - xi, 1, c);
        }
    }
}

/* 網点(ディザ)リング: step個に1ピクセルだけ打つ。半透明の代用(壁紙の透かし用) */
void ring_dither(int cx, int cy, int r_out, int r_in, uint32_t c, int step)
{
    for (int dy = -r_out; dy <= r_out; dy++)
        for (int dx = -r_out; dx <= r_out; dx++) {
            int d2 = dx * dx + dy * dy;
            if (d2 > r_out * r_out || d2 < r_in * r_in) continue;
            if (((dx + dy * 3) % step + step) % step) continue;
            px(cx + dx, cy + dy, c);
        }
}

/* 三角形: 外接矩形の各点で「3辺の同じ側にあるか」を外積の符号で判定 */
void fill_tri(int x0, int y0, int x1, int y1, int x2, int y2, uint32_t c)
{
    int minx = x0 < x1 ? (x0 < x2 ? x0 : x2) : (x1 < x2 ? x1 : x2);
    int maxx = x0 > x1 ? (x0 > x2 ? x0 : x2) : (x1 > x2 ? x1 : x2);
    int miny = y0 < y1 ? (y0 < y2 ? y0 : y2) : (y1 < y2 ? y1 : y2);
    int maxy = y0 > y1 ? (y0 > y2 ? y0 : y2) : (y1 > y2 ? y1 : y2);
    for (int y = miny; y <= maxy; y++)
        for (int x = minx; x <= maxx; x++) {
            int a = (x1 - x0) * (y - y0) - (y1 - y0) * (x - x0);
            int b = (x2 - x1) * (y - y1) - (y2 - y1) * (x - x1);
            int d = (x0 - x2) * (y - y2) - (y0 - y2) * (x - x2);
            if ((a >= 0 && b >= 0 && d >= 0) || (a <= 0 && b <= 0 && d <= 0))
                px(x, y, c);
        }
}

void draw_char(int x, int y, char ch, uint32_t fg)
{
    if (ch < 0x20 || ch > 0x7E) return;
    const uint8_t *g = font8x16[ch - 0x20];
    for (int r = 0; r < 16; r++)
        for (int b = 0; b < 8; b++)
            if (g[r] & (0x80 >> b)) px(x + b, y + r, fg);
}

void draw_str(int x, int y, const char *s, uint32_t fg)
{
    for (int i = 0; s[i]; i++) draw_char(x + i * 8, y, s[i], fg);
}

/* 1ドットをscale×scaleの正方形に拡大して描く。ロゴの大文字用 */
void draw_str_big(int x, int y, const char *s, uint32_t fg, int scale)
{
    for (int i = 0; s[i]; i++) {
        char ch = s[i];
        if (ch < 0x20 || ch > 0x7E) continue;
        const uint8_t *g = font8x16[ch - 0x20];
        for (int r = 0; r < 16; r++)
            for (int b = 0; b < 8; b++)
                if (g[r] & (0x80 >> b))
                    fill_rect(x + (i * 8 + b) * scale, y + r * scale, scale,
                              scale, fg);
    }
}

int text_w(const char *s, int scale)
{
    int n = 0;
    while (s[n]) n++;
    return n * 8 * scale;
}

void gfx_flush(void) { gfx_flush_rect(0, 0, GFX_W, GFX_H); }

void gfx_flush_rect(int x, int y, int w, int h)
{
    if (!fb) return;
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > GFX_W) w = GFX_W - x;
    if (y + h > GFX_H) h = GFX_H - y;
    if (w <= 0) return;
    for (int r = 0; r < h; r++)
        copy32((uint32_t *)(fb + (uint32_t)(y + r) * fb_pitch) + x,
               &gfx_back[(y + r) * GFX_W + x], w);
}
