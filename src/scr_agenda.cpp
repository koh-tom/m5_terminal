#include "config.h"
#include "gfx.h"
#include "schedule.h"
#include "screens.h"
#include "timekeep.h"
#include "ui.h"

static int drawDeadlines(int today, int y) {
  // 近い順に並べる
  const Deadline* list[MAX_DEADLINES];
  int n = 0;
  for (int i = 0; i < cfg.deadlineCount; i++) {
    if (ymdDiff(today, cfg.deadlines[i].ymd) >= 0) list[n++] = &cfg.deadlines[i];
  }
  for (int i = 1; i < n; i++) {
    for (int j = i; j > 0 && list[j]->ymd < list[j - 1]->ymd; j--) {
      const Deadline* t = list[j];
      list[j] = list[j - 1];
      list[j - 1] = t;
    }
  }
  if (n == 0) return y;

  int x = 12;
  for (int i = 0; i < n; i++) {
    int days = ymdDiff(today, list[i]->ymd);
    char dd[12];
    if (days == 0) {
      strcpy(dd, "D-DAY");
    } else {
      snprintf(dd, sizeof(dd), "D-%d", days);
    }
    int wt = min(textWidth(list[i]->title, Font::Jp13), 96);
    int w = wt + textWidth(dd, Font::Lat12) + 38;
    if (x + w > UI_W - 12) break;
    pill(x, y, w, 26, col::SURFACE);
    canvas.fillSmoothCircle(x + 11, y + 13, 3, deadlineColor(days));
    textFit(list[i]->title, x + 20, y + 13, 96, Font::Jp13, col::SUB, middle_left);
    text(dd, x + w - 10, y + 13, Font::Lat12, deadlineColor(days), middle_right);
    x += w + 8;
  }
  return y + 34;
}

static void dayLabel(char* buf, size_t n, int today, int ymd) {
  int d = ymdDiff(today, ymd);
  const char* rel = d == 0 ? "今日" : d == 1 ? "明日" : "";
  snprintf(buf, n, "%s%s%d/%d (%s)", rel, rel[0] ? "  " : "", ymd / 100 % 100, ymd % 100,
           WDAY[ymdWeekday(ymd)]);
}

void drawAgenda(const Frame& f) {
  if (!f.today) {
    text("時刻が未設定です", UI_W / 2, 120, Font::Jp16, col::AMBER, middle_center);
    return;
  }

  int y = drawDeadlines(f.today, UI_HEADER_H + 8);
  const int rowH = 28, bottom = UI_H - 4;

  int mtgDay = 0;
  const Item* mtg = nextMeeting(f.now, &mtgDay);
  bool any = false;

  for (int d = 0; d <= 7 && y < bottom; d++) {
    int ymd = ymdAddDays(f.today, d);
    const Item* list[MAX_ITEMS];
    int n = scheduleOfDay(ymd, list, MAX_ITEMS);

    // 今日の終わった予定は出さない
    int first = 0;
    if (d == 0) {
      while (first < n && itemEnd(list[first], ymd) <= f.now) first++;
    }
    if (first >= n) continue;
    if (y + 18 + rowH > bottom) break;

    char label[40];
    dayLabel(label, sizeof(label), f.today, ymd);
    text(label, 14, y + 8, Font::Jp13, d == 0 ? col::TEXT : col::SUB, middle_left);
    int lx = 22 + textWidth(label, Font::Jp13);
    canvas.drawFastHLine(lx, y + 8, UI_W - 14 - lx, col::LINE);
    y += 18;

    for (int i = first; i < n && y + rowH <= bottom; i++) {
      const Item* it = list[i];
      time_t st = itemStart(it, ymd), en = itemEnd(it, ymd);
      bool active = st <= f.now && f.now < en;
      bool isNext = it == mtg && ymd == mtgDay;
      int cy = y + rowH / 2;

      char hm[8];
      snprintf(hm, sizeof(hm), "%d:%02d", it->start / 60, it->start % 60);
      text(hm, 54, cy, Font::Jp16, isNext ? col::ACCENT : col::SUB, middle_right);
      pill(64, y + 6, 3, rowH - 12, active ? col::GREEN : it->mtg ? col::ACCENT : col::MUTED);

      char right[24] = "";
      uint16_t rc = col::SUB;
      if (active) {
        strcpy(right, "進行中");
        rc = col::GREEN;
      } else if (isNext && d == 0) {
        int mins = (int)((st - f.now + 59) / 60);
        if (mins >= 60) {
          snprintf(right, sizeof(right), "あと%d時間%d分", mins / 60, mins % 60);
        } else {
          snprintf(right, sizeof(right), "あと%d分", mins);
        }
        rc = col::ACCENT;
      }
      int rw = right[0] ? textWidth(right, Font::Jp13) + 20 : 0;
      textFit(it->title, 76, cy, UI_W - 76 - 14 - rw, Font::Jp16, col::TEXT, middle_left);
      if (right[0]) {
        pill(UI_W - 14 - rw + 6, cy - 11, rw - 6, 22, col::SURFACE);
        text(right, UI_W - 14 - (rw - 6) / 2, cy, Font::Jp13, rc, middle_center);
      }
      y += rowH;
      any = true;
    }
  }

  if (!any) {
    text("この先1週間の予定はありません", UI_W / 2, (y + UI_H) / 2, Font::Jp13, col::MUTED, middle_center);
  }
}
