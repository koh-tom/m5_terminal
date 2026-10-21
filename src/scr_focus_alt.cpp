// 集中画面のいろいろな表示 (長押しで切り替え)
#include <math.h>

#include "focus.h"
#include "gfx.h"
#include "screens.h"
#include "ui.h"

namespace {

bool isWork() { return focusPhase() == Phase::Work; }
bool running() { return focusState() == RunState::Running; }
uint16_t phaseColor() { return isWork() ? col::RED : col::GREEN; }

// 経過の割合 0.0 - 1.0
float progress() {
  uint32_t len = focusLengthMs();
  if (!len) return 0;
  return 1.0f - (float)focusRemainMs() / len;
}

void fmtRemain(char* buf, size_t n) {
  uint32_t sec = (focusRemainMs() + 999) / 1000;
  snprintf(buf, n, "%02lu:%02lu", (unsigned long)(sec / 60), (unsigned long)(sec % 60));
}

// 左上に状態、右上に今日の合計
void topBar() {
  const char* label = isWork() ? "研究" : focusLongBreak() ? "長めの休憩" : "休憩";
  if (focusState() == RunState::Paused) label = "一時停止中";
  if (focusState() == RunState::Idle && isWork()) label = "待機中";
  int w = textWidth(label, Font::Jp13) + 34;
  pill(12, 36, w, 24, col::SURFACE);
  canvas.fillSmoothCircle(25, 48, 3, running() ? phaseColor() : col::MUTED);
  text(label, 34, 48, Font::Jp13, col::SUB, middle_left);

  char dur[24], line[48];
  fmtDuration(dur, sizeof(dur), focusTodaySec());
  snprintf(line, sizeof(line), "今日 %s · %d回", dur, focusTodayCount());
  text(line, UI_W - 12, 48, Font::Jp13, col::MUTED, middle_right);
}

void pt(int cx, int cy, float deg, float r, int* x, int* y) {
  float a = (deg - 90) * (float)M_PI / 180.0f;
  *x = cx + cosf(a) * r;
  *y = cy + sinf(a) * r;
}

}  // namespace

// リング: 円が少しずつ埋まっていく
void drawFocusRing(const Frame&) {
  topBar();
  const int cx = UI_W / 2, cy = 126, r = 64;
  canvas.fillArc(cx, cy, r, r - 8, 0, 360, col::SURFACE2);
  float p = progress();
  if (p > 0.005f) {
    canvas.fillArc(cx, cy, r, r - 8, -90, -90 + 360 * p, phaseColor());
    int x, y;
    pt(cx, cy, 0, r - 4, &x, &y);
    canvas.fillSmoothCircle(x, y, 4, phaseColor());
    pt(cx, cy, 360 * p, r - 4, &x, &y);
    canvas.fillSmoothCircle(x, y, 4, phaseColor());
  }

  char buf[12];
  fmtRemain(buf, sizeof(buf));
  text(buf, cx, cy + 2, Font::Num40, running() ? col::TEXT : col::SUB, middle_center);
  snprintf(buf, sizeof(buf), "%d%%", (int)(p * 100));
  text(buf, cx, cy + 32, Font::Lat12, col::MUTED, middle_center);
}

// タイムタイマー風: 残りが赤い扇形で減っていく
void drawFocusDial(const Frame&) {
  topBar();
  const int cx = UI_W / 2, cy = 128, r = 66;
  canvas.fillSmoothCircle(cx, cy, r, col::SURFACE);

  int lenMin = focusLengthMs() / 60000;
  int step = lenMin > 30 ? 10 : 5;
  for (int m = 0; m < lenMin; m++) {
    float deg = 360.0f * m / lenMin;
    int x0, y0, x1, y1;
    bool major = m % step == 0;
    pt(cx, cy, deg, r - 2, &x0, &y0);
    pt(cx, cy, deg, r - (major ? 9 : 5), &x1, &y1);
    canvas.drawWideLine(x0, y0, x1, y1, major ? 1.2f : 0.6f, major ? col::SUB : col::MUTED);
  }

  // 残りの分だけ扇形
  float remain = 1.0f - progress();
  uint16_t c = running() ? phaseColor() : rgb(120, 60, 60);
  if (!isWork()) c = running() ? col::GREEN : rgb(50, 110, 80);
  if (remain > 0.002f) canvas.fillArc(cx, cy, 0, r - 12, -90, -90 + 360 * remain, c);

  int hx, hy;
  pt(cx, cy, 360 * remain, r - 12, &hx, &hy);
  canvas.drawWideLine(cx, cy, hx, hy, 1.5f, col::TEXT);
  canvas.fillSmoothCircle(cx, cy, 9, col::SURFACE2);
  canvas.fillSmoothCircle(cx, cy, 4, col::TEXT);

  // 横に残り時間
  char buf[12];
  fmtRemain(buf, sizeof(buf));
  text(buf, 20, 186, Font::Lat12, col::SUB, middle_left);
}

// 水がたまっていく
void drawFocusWater(const Frame&) {
  const int top = UI_HEADER_H, bottom = UI_FOOTER_Y;
  float p = progress();
  uint16_t water = isWork() ? rgb(40, 70, 150) : rgb(30, 110, 80);
  uint16_t foam = isWork() ? rgb(90, 130, 230) : rgb(70, 190, 130);
  if (!running()) {
    water = rgb(40, 46, 60);
    foam = rgb(70, 78, 96);
  }

  float level = bottom - (bottom - top) * p;
  float t = millis() / 600.0f;
  float amp = running() ? 3.0f : 1.0f;
  for (int x = 0; x < UI_W; x++) {
    int y = level + sinf(x / 22.0f + t) * amp + sinf(x / 9.0f - t * 1.7f) * amp * 0.4f;
    y = constrain(y, top, bottom);
    if (y < bottom) {
      canvas.drawFastVLine(x, y, bottom - y, water);
      canvas.drawPixel(x, y, foam);
    }
  }

  topBar();
  char buf[12];
  fmtRemain(buf, sizeof(buf));
  text(buf, UI_W / 2, 150, Font::Num84, col::TEXT, baseline_center);
}

// 1分ごとのマスが埋まっていく
void drawFocusGrid(const Frame&) {
  topBar();
  int lenMin = max(1, (int)(focusLengthMs() / 60000));
  float doneMin = progress() * lenMin;
  // 50分なら13x4くらいに収める
  const int gap = 4, areaW = UI_W - 24, areaH = 88;
  int cols = min(lenMin, 13);
  int rows = (lenMin + cols - 1) / cols;
  int cell = min(20, min((areaW - (cols - 1) * gap) / cols, (areaH - (rows - 1) * gap) / rows));
  int gw = cols * cell + (cols - 1) * gap;
  int gh = rows * cell + (rows - 1) * gap;
  int x0 = (UI_W - gw) / 2;
  int y0 = 100 + (areaH - gh) / 2;

  char buf[12];
  fmtRemain(buf, sizeof(buf));
  text(buf, UI_W / 2, 78, Font::Num40, running() ? col::TEXT : col::SUB, middle_center);

  for (int i = 0; i < lenMin; i++) {
    int x = x0 + (i % cols) * (cell + gap);
    int y = y0 + (i / cols) * (cell + gap);
    int rr = max(2, cell / 4);
    roundRect(x, y, cell, cell, rr, col::SURFACE2);
    if (i < (int)doneMin) {
      roundRect(x, y, cell, cell, rr, phaseColor());
    } else if (i == (int)doneMin && running()) {
      // 今の1分は下から埋める
      int h = (int)((doneMin - i) * cell);
      if (h > 1) canvas.fillRect(x, y + cell - h, cell, h, isWork() ? rgb(150, 70, 70) : rgb(50, 130, 90));
    }
  }
}

// 砂時計
void drawFocusHourglass(const Frame&) {
  topBar();
  const int cx = 226, top = 66, bottom = 188, hw = 46;
  const int neck = (top + bottom) / 2;
  const float H = neck - top;
  float remain = 1.0f - progress();
  uint16_t sand = running() ? (isWork() ? col::AMBER : col::GREEN) : rgb(120, 100, 70);

  // 上: 首の上に残りの砂 (面積が残りに比例するように高さはsqrt)
  float h = H * sqrtf(remain);
  if (h > 1) {
    int w = (int)(hw * h / H);
    canvas.fillTriangle(cx, neck, cx - w, neck - h, cx + w, neck - h, sand);
  }
  // 下: 底から積もる
  float k = H * (1.0f - sqrtf(remain));
  if (k > 1) {
    int wt = (int)(hw * (H - k) / H);
    canvas.fillTriangle(cx - hw, bottom, cx + hw, bottom, cx - wt, bottom - k, sand);
    canvas.fillTriangle(cx + hw, bottom, cx - wt, bottom - k, cx + wt, bottom - k, sand);
  }
  if (running() && remain > 0.001f) canvas.drawFastVLine(cx, neck, bottom - neck - (int)k, sand);

  // ガラス
  canvas.drawWideLine(cx - hw, top, cx + hw, top, 1.5f, col::SUB);
  canvas.drawWideLine(cx - hw, bottom, cx + hw, bottom, 1.5f, col::SUB);
  canvas.drawWideLine(cx - hw, top, cx - 3, neck, 1.0f, col::SUB);
  canvas.drawWideLine(cx + hw, top, cx + 3, neck, 1.0f, col::SUB);
  canvas.drawWideLine(cx - hw, bottom, cx - 3, neck, 1.0f, col::SUB);
  canvas.drawWideLine(cx + hw, bottom, cx + 3, neck, 1.0f, col::SUB);

  char buf[12];
  fmtRemain(buf, sizeof(buf));
  text(buf, 24, 134, Font::Num40, running() ? col::TEXT : col::SUB, baseline_left);
  text("のこり", 26, 152, Font::Jp13, col::MUTED, middle_left);
}
