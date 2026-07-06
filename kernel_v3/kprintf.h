#pragma once
/* v3のkprintfはシリアル(COM1)へ出るカーネルログ。画面表示は窓の仕事 */
void kputc(char c);
void kprintf(const char *fmt, ...);
