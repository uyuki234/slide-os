#pragma once
void mouse_init(void);
/* 現在状態(割り込みが更新、メインループが読む)。座標は画面内にクランプ済み */
extern volatile int mouse_x, mouse_y;
extern volatile int mouse_btn; /* bit0=左 bit1=右 bit2=中 */
