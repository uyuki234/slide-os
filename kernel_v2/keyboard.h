#pragma once
/* 0x100以上は「文字ではない特殊キー」。charに収まらない値を使うのがミソ */
enum { KEY_UP = 0x100, KEY_DOWN, KEY_LEFT, KEY_RIGHT };
void keyboard_init(void);
int  kbd_getchar(void);   /* ASCII、特殊キーコード、なければ-1 */
