#include "ui.h"

#include <M5Unified.h>

static M5Canvas canvas(&M5.Display);
static time_t lastDrawn = 0;

static const char* WDAY[] = {"日", "月", "火", "水", "木", "金", "土"};

static void draw(time_t now) {
  struct tm t;
  localtime_r(&now, &t);

  canvas.fillScreen(TFT_BLACK);
  canvas.setTextColor(TFT_WHITE);
  canvas.setTextDatum(middle_center);

  char buf[16];
  snprintf(buf, sizeof(buf), "%02d:%02d", t.tm_hour, t.tm_min);
  canvas.setFont(&fonts::DejaVu72);
  canvas.drawString(buf, 160, 100);

  char date[40];
  snprintf(date, sizeof(date), "%d月%d日 (%s)", t.tm_mon + 1, t.tm_mday, WDAY[t.tm_wday]);
  canvas.setFont(&fonts::lgfxJapanGothicP_24);
  canvas.drawString(date, 160, 160);

  canvas.pushSprite(0, 0);
}

void uiBegin() {
  canvas.setColorDepth(16);
  canvas.createSprite(M5.Display.width(), M5.Display.height());
}

void uiLoop() {
  time_t now = time(nullptr);
  if (now != lastDrawn) {
    lastDrawn = now;
    draw(now);
  }
}

// canvasの中身をそのまま送る (RGB565 big endian)
void uiShot() {
  int w = canvas.width(), h = canvas.height();
  Serial.printf("SHOT %d %d\n", w, h);
  Serial.write((const uint8_t*)canvas.getBuffer(), w * h * 2);
  Serial.flush();
}
