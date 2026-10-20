#include <math.h>

#include "config.h"
#include "gfx.h"
#include "screens.h"
#include "timekeep.h"
#include "ui.h"

static const char* const WDAY_EN[7] = {"SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT"};

const Deadline* nearestDeadline(int today, int* days) {
  const Deadline* best = nullptr;
  if (!today) return nullptr;
  for (int i = 0; i < cfg.deadlineCount; i++) {
    int d = ymdDiff(today, cfg.deadlines[i].ymd);
    if (d < 0) continue;
    if (!best || d < *days) {
      best = &cfg.deadlines[i];
      *days = d;
    }
  }
  return best;
}

uint16_t deadlineColor(int days) {
  return days <= 3 ? col::RED : days <= 7 ? col::AMBER : col::ACCENT;
}

static void fmtDday(char* buf, size_t n, int days) {
  if (days == 0) {
    strlcpy(buf, "D-DAY", n);
  } else {
    snprintf(buf, n, "D-%d", days);
  }
}

static void drawDigital(const Frame& f) {
  char hh[4], mm[4];
  snprintf(hh, sizeof(hh), "%02d", f.t.tm_hour);
  snprintf(mm, sizeof(mm), "%02d", f.t.tm_min);

  canvas.setFont(font(Font::Clock));
  int wh = canvas.textWidth(hh), wc = canvas.textWidth(":"), wm = canvas.textWidth(mm);
  int x = (UI_W - wh - wc - wm) / 2;
  const int base = 148;
  text(hh, x, base, Font::Clock, col::TEXT, baseline_left);
  text(":", x + wh, base - 4, Font::Clock, col::ACCENT, baseline_left);
  text(mm, x + wh + wc, base, Font::Clock, col::TEXT, baseline_left);

  // 秒は細いバーで
  const int bx = 70, bw = UI_W - 140, by = 168;
  pill(bx, by, bw, 3, col::SURFACE2);
  pill(bx, by, bw * (f.t.tm_sec + 1) / 60, 3, col::ACCENT);

  char date[48];
  snprintf(date, sizeof(date), "%d月%d日  %s曜日", f.t.tm_mon + 1, f.t.tm_mday, WDAY[f.t.tm_wday]);
  text(date, 18, 214, Font::Jp16, col::SUB, middle_left);

  int days = 0;
  const Deadline* dl = nearestDeadline(f.today, &days);
  if (dl) {
    char dd[12];
    fmtDday(dd, sizeof(dd), days);
    int wd = textWidth(dd, Font::Lat12);
    int wt = min(textWidth(dl->title, Font::Jp13), 110);
    int w = wt + wd + 30, px = UI_W - 14 - w;
    pill(px, 201, w, 26, col::SURFACE);
    textFit(dl->title, px + 12, 214, 110, Font::Jp13, col::SUB, middle_left);
    text(dd, px + w - 12, 214, Font::Lat12, deadlineColor(days), middle_right);
  }
}

static void hand(int cx, int cy, float deg, float len, float tail, float w0, float w1, uint16_t c) {
  float a = deg * (float)M_PI / 180.0f;
  float s = sinf(a), k = cosf(a);
  canvas.drawWedgeLine(cx - s * tail, cy + k * tail, cx + s * len, cy - k * len, w0, w1, c);
}

static void drawAnalog(const Frame& f) {
  const int cx = UI_W / 2, cy = 134, r = 100;
  canvas.fillSmoothCircle(cx, cy, r, col::SURFACE);

  for (int i = 0; i < 60; i++) {
    float a = i * 6 * (float)M_PI / 180.0f;
    float s = sinf(a), k = cosf(a);
    if (i % 5 == 0) {
      float w = i % 15 == 0 ? 1.6f : 1.0f;
      canvas.drawWideLine(cx + s * (r - 16), cy - k * (r - 16), cx + s * (r - 7), cy - k * (r - 7), w,
                          i % 15 == 0 ? col::TEXT : col::SUB);
    } else {
      canvas.fillSmoothCircle(cx + s * (r - 8), cy - k * (r - 8), 1, col::MUTED);
    }
  }

  // 文字盤の中の情報
  char buf[16];
  snprintf(buf, sizeof(buf), "%s %d", WDAY_EN[f.t.tm_wday], f.t.tm_mday);
  text(buf, cx, cy + 42, Font::Lat12, col::SUB, middle_center);
  int days = 0;
  if (nearestDeadline(f.today, &days)) {
    fmtDday(buf, sizeof(buf), days);
    text(buf, cx, cy - 42, Font::Lat12, deadlineColor(days), middle_center);
  }

  float sec = f.t.tm_sec;
  float min = f.t.tm_min + sec / 60.0f;
  float hour = (f.t.tm_hour % 12) + min / 60.0f;
  hand(cx, cy, hour * 30, r * 0.5f, 0, 4.0f, 2.0f, col::TEXT);
  hand(cx, cy, min * 6, r * 0.8f, 0, 3.0f, 1.5f, col::TEXT);
  hand(cx, cy, sec * 6, r * 0.86f, r * 0.18f, 1.0f, 1.0f, col::RED);
  canvas.fillSmoothCircle(cx, cy, 5, col::RED);
  canvas.fillSmoothCircle(cx, cy, 2, col::SURFACE);
}

void drawClock(const Frame& f, bool analog) {
  if (!f.today) {
    text("時刻が未設定です", UI_W / 2, 110, Font::Jp20, col::AMBER, middle_center);
    text("wifiを設定するか settime.py を実行", UI_W / 2, 146, Font::Jp13, col::SUB, middle_center);
    return;
  }
  if (analog) {
    drawAnalog(f);
  } else {
    drawDigital(f);
  }
}
