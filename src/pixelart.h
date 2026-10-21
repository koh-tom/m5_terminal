#pragma once
#include <stddef.h>
#include <stdint.h>

// 横向きの時に出すドット絵。tools/push_image.py で内蔵flashに書き込む
//   形式: "PX01" + 幅(u16 LE) + 高さ(u16 LE) + RGB565(big endian)
#define ART_MAX 240

bool artBegin();
bool artAvailable();
int artWidth();
int artHeight();
uint16_t artPixel(int x, int y);

bool artSave(const uint8_t* data, size_t len);
void artClear();
