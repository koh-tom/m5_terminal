#include "focus.h"
#include "gfx.h"
#include "screens.h"
#include "ui.h"

static void fmtMs(char* buf, size_t n, uint32_t ms) {
  uint32_t sec = (ms + 999) / 1000;
  snprintf(buf, n, "%02lu:%02lu", (unsigned long)(sec / 60), (unsigned long)(sec % 60));
}

void drawFocus(const Frame& f) {
  (void)f;
  bool work = focusPhase() == Phase::Work;
  RunState st = focusState();
  uint16_t color = work ? col::RED : col::GREEN;

  const char* label = work ? "研究" : "休憩";
  if (st == RunState::Paused) label = "一時停止中";
  if (st == RunState::Idle && work) label = "待機中";
  int w = textWidth(label, Font::Jp13) + 34;
  pill((UI_W - w) / 2, 38, w, 24, col::SURFACE);
  canvas.fillSmoothCircle((UI_W - w) / 2 + 13, 50, 3, st == RunState::Running ? color : col::MUTED);
  text(label, (UI_W - w) / 2 + 22, 50, Font::Jp13, col::SUB, middle_left);

  char buf[16];
  fmtMs(buf, sizeof(buf), focusRemainMs());
  text(buf, UI_W / 2, 142, Font::Num84, st == RunState::Running ? col::TEXT : col::SUB, baseline_center);

  const int bw = 220, bx = (UI_W - bw) / 2, by = 158;
  uint32_t len = focusLengthMs();
  int done = len ? (int)((uint64_t)(len - focusRemainMs()) * bw / len) : 0;
  pill(bx, by, bw, 4, col::SURFACE2);
  if (done > 3) pill(bx, by, done, 4, color);

  char dur[24], line[64];
  fmtDuration(dur, sizeof(dur), focusTodaySec());
  snprintf(line, sizeof(line), "今日 %s  ·  %d回", dur, focusTodayCount());
  text(line, UI_W / 2, 180, Font::Jp13, col::MUTED, middle_center);
}

static void iconPlay(int x, int y, uint16_t c) { canvas.fillTriangle(x, y - 6, x, y + 6, x + 10, y, c); }

static void iconPause(int x, int y, uint16_t c) {
  canvas.fillRect(x, y - 6, 3, 12, c);
  canvas.fillRect(x + 6, y - 6, 3, 12, c);
}

static void iconReset(int x, int y, uint16_t c) {
  canvas.fillArc(x + 5, y, 6, 4, 0, 270, c);
  canvas.fillTriangle(x + 8, y, x + 14, y, x + 11, y + 4, c);
}

void drawFocusFooter() {
  bool running = focusState() == RunState::Running;
  const char* labels[] = {"開始", "停止", "リセット"};
  const int bw = 92, h = 32, y = UI_FOOTER_Y + 6;
  for (int i = 0; i < 3; i++) {
    int x = 10 + i * (bw + 12);
    bool primary = (i == 0 && !running) || (i == 1 && running);
    uint16_t bg = primary ? col::ACCENT : col::SURFACE;
    uint16_t fg = primary ? col::BG : col::TEXT;
    pill(x, y, bw, h, bg);

    int tw = textWidth(labels[i], Font::Jp13);
    int ix = x + (bw - tw - 18) / 2, cy = y + h / 2;
    if (i == 0) iconPlay(ix, cy, fg);
    if (i == 1) iconPause(ix, cy, fg);
    if (i == 2) iconReset(ix - 2, cy, fg);
    text(labels[i], ix + 18, cy, Font::Jp13, fg, middle_left);
  }
}
