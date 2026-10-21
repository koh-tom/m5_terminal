// 集中画面のエヴァ風表示 (内部電源の活動限界タイマー)
#include "focus.h"
#include "gfx.h"
#include "screens.h"
#include "ui.h"

namespace {
constexpr uint16_t EVA_ORANGE = rgb(255, 122, 0);
constexpr uint16_t EVA_ORANGE_DIM = rgb(110, 52, 0);
constexpr uint16_t EVA_GHOST = rgb(24, 10, 0);
constexpr uint16_t EVA_RED = rgb(255, 36, 36);
constexpr uint16_t EVA_GREEN = rgb(70, 255, 120);
constexpr uint16_t EVA_GREEN_DIM = rgb(20, 80, 40);

// bit0=a ... bit6=g
const uint8_t SEG[10] = {0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07, 0x7F, 0x6F};

void hseg(int x0, int x1, int y, int t, uint16_t c) {
  int h = t / 2;
  canvas.fillRect(x0 + h, y - h, x1 - x0 - 2 * h, t, c);
  canvas.fillTriangle(x0, y, x0 + h, y - h, x0 + h, y + h - 1, c);
  canvas.fillTriangle(x1, y, x1 - h, y - h, x1 - h, y + h - 1, c);
}

void vseg(int x, int y0, int y1, int t, uint16_t c) {
  int h = t / 2;
  canvas.fillRect(x - h, y0 + h, t, y1 - y0 - 2 * h, c);
  canvas.fillTriangle(x, y0, x - h, y0 + h, x + h - 1, y0 + h, c);
  canvas.fillTriangle(x, y1, x - h, y1 - h, x + h - 1, y1 - h, c);
}

// 7セグ1桁。消えてるセグメントもうっすら出す
void digit(int x, int y, int w, int h, int t, int n, uint16_t on) {
  uint8_t bits = SEG[n % 10];
  int g = 1, m = y + h / 2;
  int l = x + t / 2, r = x + w - t / 2;
  auto c = [&](int b) { return (bits >> b) & 1 ? on : EVA_GHOST; };
  hseg(l + g, r - g, y + t / 2, t, c(0));
  vseg(r, y + t / 2 + g, m - g, t, c(1));
  vseg(r, m + g, y + h - t / 2 - g, t, c(2));
  hseg(l + g, r - g, y + h - t / 2, t, c(3));
  vseg(l, m + g, y + h - t / 2 - g, t, c(4));
  vseg(l, y + t / 2 + g, m - g, t, c(5));
  hseg(l + g, r - g, m, t, c(6));
}

void colon(int x, int y, int h, int t, uint16_t c) {
  canvas.fillRect(x, y + h * 3 / 10 - t / 2, t, t, c);
  canvas.fillRect(x, y + h * 7 / 10 - t / 2, t, t, c);
}

void lamp(int x, int y, const char* label, bool lit, uint16_t c) {
  const int w = 40, h = 20;
  if (lit) {
    canvas.fillRect(x, y, w, h, c);
  } else {
    canvas.drawRect(x, y, w, h, EVA_ORANGE_DIM);
  }
  text(label, x + w / 2, y + h / 2, Font::Jp13, lit ? TFT_BLACK : EVA_ORANGE_DIM, middle_center);
}

void hazard(int y, int h, uint16_t c) {
  for (int x = -h; x < UI_W; x += 16) {
    canvas.fillTriangle(x, y + h, x + h, y, x + h + 8, y, c);
    canvas.fillTriangle(x, y + h, x + 8, y + h, x + h + 8, y, c);
  }
}
}  // namespace

void drawEva(const Frame& f) {
  (void)f;
  canvas.fillRect(0, UI_HEADER_H, UI_W, UI_H - UI_HEADER_H, TFT_BLACK);

  bool work = focusPhase() == Phase::Work;
  RunState st = focusState();
  uint32_t ms = focusRemainMs();
  bool warn = work && st == RunState::Running && ms < 60000;
  bool blink = (millis() / 250) % 2;

  uint16_t main = work ? EVA_ORANGE : EVA_GREEN;
  uint16_t dim = work ? EVA_ORANGE_DIM : EVA_GREEN_DIM;
  if (warn) main = blink ? EVA_RED : rgb(120, 0, 0);

  const char* title = st == RunState::Idle ? "待機中" : work ? "活動限界まで" : "活動再開まで";
  text(title, 12, 47, Font::Eva24, work ? EVA_ORANGE : EVA_GREEN, middle_left);

  lamp(184, 37, "内部", work && st == RunState::Running, EVA_ORANGE);
  lamp(228, 37, "外部", !work, EVA_GREEN);
  lamp(272, 37, "停止", st != RunState::Running, EVA_RED);

  text(work ? "ACTIVE TIME REMAINING" : "EXTERNAL POWER CONNECTED", 12, 70, Font::Lat12, dim, middle_left);
  canvas.drawFastHLine(12, 78, UI_W - 24, dim);

  // MM:SS:cc
  uint32_t cs = ms / 10;
  uint32_t mm = cs / 6000, ss = cs / 100 % 60, cc = cs % 100;
  const int W = 34, H = 64, T = 7, G = 6, CW = 12;
  const int w2 = 20, h2 = 36, t2 = 5;
  int mdig = mm >= 100 ? 3 : 2;
  int total = (W + G) * mdig + CW + (W + G) * 2 + CW + (w2 + 4) * 2;
  int x = (UI_W - total) / 2, y = 86;
  for (int i = mdig - 1; i >= 0; i--) {
    int div = i == 2 ? 100 : i == 1 ? 10 : 1;
    digit(x, y, W, H, T, mm / div % 10, main);
    x += W + G;
  }
  colon(x, y, H, T, main);
  x += CW;
  digit(x, y, W, H, T, ss / 10, main);
  x += W + G;
  digit(x, y, W, H, T, ss % 10, main);
  x += W + G;
  colon(x, y + H - h2, h2, t2, main);
  x += CW;
  digit(x, y + H - h2, w2, h2, t2, cc / 10, main);
  digit(x + w2 + 4, y + H - h2, w2, h2, t2, cc % 10, main);

  // 残量ゲージ
  const int blocks = 30, bw = 8, bx = (UI_W - blocks * bw) / 2, by = 160;
  uint32_t len = focusLengthMs();
  int lit = len ? (int)((uint64_t)ms * blocks / len + (ms > 0 ? 1 : 0)) : 0;
  for (int i = 0; i < blocks; i++) {
    canvas.fillRect(bx + i * bw, by, bw - 2, 10, i < lit ? main : EVA_GHOST);
  }

  char dur[24], line[64];
  fmtDuration(dur, sizeof(dur), focusTodaySec());
  snprintf(line, sizeof(line), "本日稼働 %s / %d回", dur, focusTodayCount());
  text(line, 12, 184, Font::Jp13, dim, middle_left);

  if (warn) hazard(UI_FOOTER_Y - 8, 6, blink ? EVA_RED : rgb(90, 0, 0));
}

void drawEvaFooter() {
  bool running = focusState() == RunState::Running;
  const char* labels[] = {"起動", "停止", "初期化"};
  const int bw = 96, h = 32, y = UI_FOOTER_Y + 6;
  for (int i = 0; i < 3; i++) {
    int x = 8 + i * (bw + 8);
    bool lit = (i == 0 && running) || (i == 1 && !running && focusState() == RunState::Paused);
    if (lit) {
      canvas.fillRect(x, y, bw, h, EVA_ORANGE);
    } else {
      canvas.drawRect(x, y, bw, h, EVA_ORANGE);
    }
    text(labels[i], x + bw / 2, y + h / 2, Font::Jp13, lit ? TFT_BLACK : EVA_ORANGE, middle_center);
  }
}
