#include "ui.h"

#include <M5Unified.h>

#include "focus.h"

static constexpr uint16_t rgb(uint8_t r, uint8_t g, uint8_t b) {
  return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}

static constexpr uint16_t C_BG = rgb(16, 18, 24);
static constexpr uint16_t C_PANEL = rgb(40, 44, 56);
static constexpr uint16_t C_TEXT = rgb(235, 235, 235);
static constexpr uint16_t C_DIM = rgb(130, 136, 150);
static constexpr uint16_t C_WORK = rgb(235, 80, 70);
static constexpr uint16_t C_BREAK = rgb(70, 190, 120);

static constexpr int W = 320;
static constexpr int FOOTER_Y = UI_FOOTER_Y;

static M5Canvas canvas(&M5.Display);
static Screen screen = Screen::Clock;
static bool dirty = true;
static time_t lastNow = 0;
static uint32_t lastRemainSec = 0;

static const char* WDAY[] = {"日", "月", "火", "水", "木", "金", "土"};

static void drawFooter(const char* a, const char* b, const char* c) {
  const char* labels[] = {a, b, c};
  canvas.setFont(&fonts::lgfxJapanGothicP_16);
  canvas.setTextDatum(middle_center);
  for (int i = 0; i < 3; i++) {
    int x = i * W / 3;
    canvas.fillRoundRect(x + 6, FOOTER_Y + 6, W / 3 - 12, 30, 6, C_PANEL);
    canvas.setTextColor(C_TEXT);
    canvas.drawString(labels[i], x + W / 6, FOOTER_Y + 22);
  }
}

static void drawClock(const struct tm& t) {
  canvas.setTextColor(C_TEXT);
  canvas.setTextDatum(middle_center);

  char buf[16];
  snprintf(buf, sizeof(buf), "%02d:%02d", t.tm_hour, t.tm_min);
  canvas.setFont(&fonts::DejaVu72);
  canvas.drawString(buf, W / 2, 80);

  char date[40];
  snprintf(date, sizeof(date), "%d月%d日 (%s)", t.tm_mon + 1, t.tm_mday, WDAY[t.tm_wday]);
  canvas.setFont(&fonts::lgfxJapanGothicP_24);
  canvas.drawString(date, W / 2, 145);
}

static void drawFocus() {
  bool work = focusPhase() == Phase::Work;
  uint16_t color = work ? C_WORK : C_BREAK;

  canvas.setTextDatum(middle_center);
  canvas.setFont(&fonts::lgfxJapanGothicP_24);
  canvas.setTextColor(color);
  canvas.drawString(work ? "研究" : "休憩", W / 2, 28);

  uint32_t sec = (focusRemainMs() + 999) / 1000;
  char buf[16];
  snprintf(buf, sizeof(buf), "%02lu:%02lu", (unsigned long)(sec / 60), (unsigned long)(sec % 60));
  canvas.setFont(&fonts::DejaVu72);
  canvas.setTextColor(focusState() == RunState::Running ? C_TEXT : C_DIM);
  canvas.drawString(buf, W / 2, 100);

  // 進捗バー
  int bw = 240, bx = (W - bw) / 2, by = 150;
  uint32_t len = focusLengthMs();
  int done = len ? (int)((uint64_t)(len - focusRemainMs()) * bw / len) : 0;
  canvas.fillRoundRect(bx, by, bw, 8, 4, C_PANEL);
  if (done > 0) canvas.fillRoundRect(bx, by, done, 8, 4, color);

  if (focusState() == RunState::Paused) {
    canvas.setFont(&fonts::lgfxJapanGothicP_16);
    canvas.setTextColor(C_DIM);
    canvas.drawString("一時停止中", W / 2, 178);
  }
}

static void draw(time_t now) {
  struct tm t;
  localtime_r(&now, &t);

  canvas.fillScreen(C_BG);
  switch (screen) {
    case Screen::Clock: drawClock(t); break;
    case Screen::Focus: drawFocus(); break;
    default: break;
  }
  drawFooter("開始", "停止", "リセット");
  canvas.pushSprite(0, 0);
}

void uiBegin() {
  canvas.setColorDepth(16);
  canvas.createSprite(M5.Display.width(), M5.Display.height());
}

void uiLoop() {
  time_t now = time(nullptr);
  uint32_t remainSec = (focusRemainMs() + 999) / 1000;
  if (dirty || now != lastNow || remainSec != lastRemainSec) {
    dirty = false;
    lastNow = now;
    lastRemainSec = remainSec;
    draw(now);
  }
}

void uiDirty() { dirty = true; }

Screen uiScreen() { return screen; }

void uiMoveScreen(int dir) {
  int n = (int)Screen::Count;
  screen = (Screen)(((int)screen + dir + n) % n);
  dirty = true;
}

void uiNextScreen() { uiMoveScreen(1); }

// canvasの中身をそのまま送る (RGB565 big endian)
void uiShot() {
  int w = canvas.width(), h = canvas.height();
  Serial.printf("SHOT %d %d\n", w, h);
  Serial.write((const uint8_t*)canvas.getBuffer(), w * h * 2);
  Serial.flush();
}
