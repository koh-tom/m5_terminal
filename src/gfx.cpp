#include "gfx.h"

const char* const WDAY[7] = {"日", "月", "火", "水", "木", "金", "土"};

static void setFont(const char* s, Font f) {
  if (fontHas(f, s)) {
    canvas.setFont(font(f));
  } else {
    canvas.setFont(&fonts::lgfxJapanGothicP_16);
  }
}

void text(const char* s, int x, int y, Font f, uint16_t color, textdatum_t datum) {
  setFont(s, f);
  canvas.setTextColor(color);
  canvas.setTextDatum(datum);
  canvas.drawString(s, x, y);
}

int textWidth(const char* s, Font f) {
  setFont(s, f);
  return canvas.textWidth(s);
}

void textFit(const char* s, int x, int y, int maxW, Font f, uint16_t color, textdatum_t datum) {
  if (textWidth(s, f) <= maxW) {
    text(s, x, y, f, color, datum);
    return;
  }
  char buf[96];
  strlcpy(buf, s, sizeof(buf) - 3);  // …の分をあけておく
  size_t len = strlen(buf);
  while (len > 0) {
    do { len--; } while (len > 0 && (buf[len] & 0xC0) == 0x80);
    strcpy(buf + len, "…");
    if (textWidth(buf, f) <= maxW) break;
  }
  text(buf, x, y, f, color, datum);
}

void pill(int x, int y, int w, int h, uint16_t color) {
  canvas.fillSmoothRoundRect(x, y, w, h, h / 2, color);
}

void roundRect(int x, int y, int w, int h, int r, uint16_t color) {
  canvas.fillSmoothRoundRect(x, y, w, h, r, color);
}

void fmtDuration(char* buf, size_t n, uint32_t sec) {
  uint32_t m = sec / 60;
  if (m >= 60) {
    snprintf(buf, n, "%lu時間%02lu分", (unsigned long)(m / 60), (unsigned long)(m % 60));
  } else {
    snprintf(buf, n, "%lu分", (unsigned long)m);
  }
}
