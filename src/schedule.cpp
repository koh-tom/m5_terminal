#include "schedule.h"

#include "beep.h"
#include "focus.h"
#include "timekeep.h"

static bool onDay(const Item& it, int ymd, int wday) {
  if (it.ymd) return it.ymd == ymd;
  return (it.dows >> wday) & 1;
}

int scheduleOfDay(int ymd, const Item** out, int max) {
  int wday = ymdWeekday(ymd);
  int n = 0;
  for (int i = 0; i < cfg.itemCount && n < max; i++) {
    if (onDay(cfg.items[i], ymd, wday)) out[n++] = &cfg.items[i];
  }
  // 挿入ソート (数が少ないので)
  for (int i = 1; i < n; i++) {
    const Item* x = out[i];
    int j = i - 1;
    while (j >= 0 && out[j]->start > x->start) {
      out[j + 1] = out[j];
      j--;
    }
    out[j + 1] = x;
  }
  return n;
}

time_t itemStart(const Item* it, int ymd) {
  struct tm t = {};
  t.tm_year = ymd / 10000 - 1900;
  t.tm_mon = ymd / 100 % 100 - 1;
  t.tm_mday = ymd % 100;
  t.tm_hour = it->start / 60;
  t.tm_min = it->start % 60;
  t.tm_isdst = -1;
  return mktime(&t);
}

time_t itemEnd(const Item* it, int ymd) {
  // 終わりが無いのは1時間扱い
  int len = it->end >= 0 ? it->end - it->start : 60;
  return itemStart(it, ymd) + len * 60;
}

const Item* nextMeeting(time_t now, int* ymd) {
  int today = todayYmd();
  if (!today) return nullptr;
  for (int d = 0; d <= 7; d++) {
    int day = ymdAddDays(today, d);
    const Item* list[MAX_ITEMS];
    int n = scheduleOfDay(day, list, MAX_ITEMS);
    for (int i = 0; i < n; i++) {
      if (list[i]->mtg && itemStart(list[i], day) > now) {
        *ymd = day;
        return list[i];
      }
    }
  }
  return nullptr;
}

static const Item* remindItem = nullptr;
static int remindDay = 0;
static time_t lastBeeped = 0;
static time_t lastCheck = 0;
static time_t pendingStart = 0;

void reminderLoop() {
  time_t now = time(nullptr);
  if (now == lastCheck) return;
  lastCheck = now;

  remindItem = nullptr;

  // MTGが始まったらタイマーを止める
  if (pendingStart && now >= pendingStart) {
    pendingStart = 0;
    if (cfg.pauseOnMtg && focusPhase() == Phase::Work && focusState() == RunState::Running) {
      focusPause();
    }
  }

  int day = 0;
  const Item* it = nextMeeting(now, &day);
  if (!it) return;
  time_t st = itemStart(it, day);
  pendingStart = st;
  if (cfg.remindMin <= 0 || st - now > cfg.remindMin * 60) return;

  remindItem = it;
  remindDay = day;
  if (lastBeeped != st) {
    lastBeeped = st;
    beep(2, 2600);
  }
}

const Item* reminderActive(int* ymd) {
  if (remindItem && ymd) *ymd = remindDay;
  return remindItem;
}
