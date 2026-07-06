#pragma once
#include <stdint.h>

/* ==== M2: 描画プリミティブ ====
 * 色は 0xRRGGBB。全ての描画は裏画面(gfx_back)に対して行い、
 * gfx_flush()/gfx_flush_rect() で初めて本物のフレームバッファに転写される */

enum { GFX_W = 1024, GFX_H = 768 };

extern uint32_t gfx_back[GFX_W * GFX_H]; /* 裏画面(合成器が直接触ってよい) */

int  gfx_init(uint32_t magic, uint32_t mbinfo); /* 0=成功 */

void px(int x, int y, uint32_t c);
void fill_rect(int x, int y, int w, int h, uint32_t c);
void fill_vgrad(int x, int y, int w, int h, uint32_t top, uint32_t bot);
void fill_circle(int cx, int cy, int r, uint32_t c);
void ring(int cx, int cy, int r_out, int r_in, uint32_t c);
void ring_dither(int cx, int cy, int r_out, int r_in, uint32_t c, int step);
void fill_tri(int x0, int y0, int x1, int y1, int x2, int y2, uint32_t c);

void draw_char(int x, int y, char ch, uint32_t fg);      /* 8x16、背景透過 */
void draw_str(int x, int y, const char *s, uint32_t fg);
void draw_str_big(int x, int y, const char *s, uint32_t fg, int scale);
int  text_w(const char *s, int scale);

void gfx_flush(void);                              /* 裏画面→フレームバッファ全転写 */
void gfx_flush_rect(int x, int y, int w, int h);   /* 部分転写(アニメーション用) */

/* rep movsl による32bit単位の高速コピー(自作memcpyの代わり) */
static inline void copy32(uint32_t *dst, const uint32_t *src, int n)
{
    __asm__ volatile("rep movsl"
                     : "+D"(dst), "+S"(src), "+c"(n)
                     :
                     : "memory");
}
