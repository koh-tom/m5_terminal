#include "focus.h"
#include "gfx.h"
#include "screens.h"
#include "timekeep.h"
#include "ui.h"

void drawStats(const Frame& f) {
  uint32_t week[7];
  focusWeek(week);

  uint32_t total = 0, maxSec = 3600;
  for (uint32_t v : week) {
    total += v;
    if (v > maxSec) maxSec = v;
  }

  char buf[48];
  text("この7日間", 14, 46, Font::Jp16, col::TEXT, middle_left);
  snprintf(buf, sizeof(buf), "合計 %.1fh", total / 3600.0f);
  text(buf, UI_W - 14, 46, Font::Jp13, col::SUB, middle_right);

  const int top = 82, bottom = 200, bw = 24;
  const int gap = (UI_W - 28 - bw * 7) / 6;
  const int x0 = 14;

  // 平均の線
  uint32_t avg = total / 7;
  if (total < 60) {
    text("まだ記録がありません", UI_W / 2, 140, Font::Jp13, col::MUTED, middle_center);
  } else if (avg >= 180) {
    int ay = bottom - (int)((uint64_t)avg * (bottom - top) / maxSec);
    for (int x = x0; x < UI_W - 14; x += 6) canvas.drawFastHLine(x, ay, 3, col::LINE);
    snprintf(buf, sizeof(buf), "avg %.1f", avg / 3600.0f);
    text(buf, UI_W - 14, ay - 8, Font::Lat12, col::MUTED, middle_right);
  }

  for (int i = 0; i < 7; i++) {
    int x = x0 + i * (bw + gap);
    int h = (int)((uint64_t)week[i] * (bottom - top) / maxSec);
    uint16_t c = i == 6 ? col::ACCENT : rgb(60, 72, 110);
    if (week[i] > 0) roundRect(x, bottom - max(h, 6), bw, max(h, 6), 6, c);
    canvas.drawFastHLine(x, bottom + 2, bw, col::SURFACE2);

    if (week[i] >= 180) {
      snprintf(buf, sizeof(buf), "%.1f", week[i] / 3600.0f);
      text(buf, x + bw / 2, bottom - max(h, 6) - 8, Font::Lat12, i == 6 ? col::TEXT : col::SUB,
           middle_center);
    }
    if (f.today) {
      int wd = ymdWeekday(ymdAddDays(f.today, i - 6));
      text(WDAY[wd], x + bw / 2, 218, Font::Jp13, i == 6 ? col::TEXT : col::MUTED, middle_center);
    }
  }
}
