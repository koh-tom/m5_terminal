#include "config.h"
#include "focus.h"
#include "gfx.h"
#include "screens.h"
#include "timekeep.h"
#include "ui.h"

#if 0  // 目標と連続日数はいらないので止めておく
// 今日(研究してなければ昨日)から何日続いているか。7日分しか見られない
static int streak(const uint32_t week[7]) {
  int i = week[6] > 0 ? 6 : 5;
  int n = 0;
  while (i >= 0 && week[i] > 0) {
    n++;
    i--;
  }
  return n;
}

static void drawGoal(uint32_t today) {
  uint32_t goal = cfg.goalMin * 60;
  float p = min(1.0f, (float)today / goal);
  const int cx = 34, cy = 60, r = 20;
  canvas.fillArc(cx, cy, r, r - 5, 0, 360, col::SURFACE2);
  if (p > 0.01f) canvas.fillArc(cx, cy, r, r - 5, -90, -90 + 360 * p, p >= 1 ? col::GREEN : col::ACCENT);
  char buf[48];
  snprintf(buf, sizeof(buf), "%d", (int)(p * 100));
  text(buf, cx, cy, Font::Lat12, col::TEXT, middle_center);

  snprintf(buf, sizeof(buf), "今日 %.1fh / %.0fh", today / 3600.0f, goal / 3600.0f);
  text(buf, 64, 51, Font::Jp16, col::TEXT, middle_left);
  if (today >= goal) {
    text("目標達成", 64, 72, Font::Jp13, col::GREEN, middle_left);
  } else {
    char dur[24];
    fmtDuration(dur, sizeof(dur), goal - today);
    snprintf(buf, sizeof(buf), "あと %s", dur);
    text(buf, 64, 72, Font::Jp13, col::MUTED, middle_left);
  }
}
#endif

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

#if 0  // 目標と連続日数
  drawGoal(week[6]);
  int st = streak(week);
  snprintf(buf, sizeof(buf), "%d", st);
  text(buf, UI_W - 52, 64, Font::Num40, st > 0 ? col::AMBER : col::MUTED, baseline_right);
  text(st >= 7 ? "日以上" : "日連続", UI_W - 48, 58, Font::Jp13, col::SUB, baseline_left);
#endif

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
