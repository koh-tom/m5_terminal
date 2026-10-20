#pragma once
#include <M5Unified.h>

#include "fonts.h"

constexpr uint16_t rgb(uint8_t r, uint8_t g, uint8_t b) {
  return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}

namespace col {
constexpr uint16_t BG = rgb(11, 13, 18);
constexpr uint16_t SURFACE = rgb(23, 27, 35);
constexpr uint16_t SURFACE2 = rgb(34, 40, 52);
constexpr uint16_t LINE = rgb(44, 51, 66);
constexpr uint16_t TEXT = rgb(236, 239, 244);
constexpr uint16_t SUB = rgb(154, 163, 178);
constexpr uint16_t MUTED = rgb(92, 101, 116);
constexpr uint16_t ACCENT = rgb(124, 156, 255);
constexpr uint16_t GREEN = rgb(79, 209, 139);
constexpr uint16_t AMBER = rgb(245, 181, 71);
constexpr uint16_t RED = rgb(255, 107, 107);
}  // namespace col

extern M5Canvas canvas;

// 文字。フォントに無い文字があればビットマップのゴシックで描く
void text(const char* s, int x, int y, Font f, uint16_t color, textdatum_t datum = top_left);
int textWidth(const char* s, Font f);

// 幅に収まらなければ後ろを…にする
void textFit(const char* s, int x, int y, int maxW, Font f, uint16_t color,
             textdatum_t datum = top_left);

void pill(int x, int y, int w, int h, uint16_t color);
void roundRect(int x, int y, int w, int h, int r, uint16_t color);

// 3:25 → "3時間25分"
void fmtDuration(char* buf, size_t n, uint32_t sec);

extern const char* const WDAY[7];
