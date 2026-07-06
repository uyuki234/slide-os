#!/usr/bin/env python3
"""vgabios-stdvga.bin から 8x16 フォント(ASCII 95文字)を抽出して font8x16.h を生成。
使い方: python3 tools/extract_font.py > kernel_v3/font8x16.h
オフセット 0x7460 は ROM 全域のストライド16スキャン(空白全零+英字グリフ密度)で特定。"""
ROM = "/opt/homebrew/share/qemu/vgabios-stdvga.bin"
T = 0x7460
rom = open(ROM, "rb").read()
print("#pragma once")
print("#include <stdint.h>")
print("static const uint8_t font8x16[95][16] = {")
for i in range(95):
    g = rom[T + i*16 : T + i*16 + 16]
    print("    {" + ", ".join(f"0x{b:02X}" for b in g) + "},")
print("};")
