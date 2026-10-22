#include "focus.h"

#include <M5Unified.h>
#include <Preferences.h>

#include "beep.h"
#include "config.h"
#include "storage.h"
#include "timekeep.h"

static Phase phase = Phase::Work;
static RunState state = RunState::Idle;
static uint32_t remainMs = 0;
static uint32_t lastTick = 0;

static Preferences prefs;
static int day = 0;
static uint32_t todayMs = 0;
static int todayCount = 0;
static uint32_t lastSave = 0;

// 過去の日ごとの合計 (古い順)
struct DayTotal {
  int32_t day;
  uint32_t sec;
};
static DayTotal hist[7];

static void pushHistory(int d, uint32_t sec) {
  memmove(&hist[0], &hist[1], sizeof(DayTotal) * 6);
  hist[6] = {d, sec};
  prefs.putBytes("hist", hist, sizeof(hist));
}

// 連続で研究していた区間
static time_t segStart = 0;
static uint32_t segMs = 0;

static void logSegment(bool done) {
  if (segStart != 0 && segMs >= 30000) {
    char a[24], b[24], line[80];
    fmtTime(segStart, a, sizeof(a));
    fmtTime(time(nullptr), b, sizeof(b));
    time_t end = time(nullptr);
    snprintf(line, sizeof(line), "%s,%s,%lu,%d", a, b, (unsigned long)(segMs / 1000), done ? 1 : 0);
    logAppend("/labterm/focus.csv", line, "start,end,sec,done");
    // PC (labterm-presence) に知らせる
    Serial.printf("@labterm {\"type\":\"focus\",\"start\":%lld,\"end\":%lld,\"sec\":%lu,\"done\":%s}\n",
                  (long long)segStart, (long long)end, (unsigned long)(segMs / 1000), done ? "true" : "false");
  }
  segStart = 0;
  segMs = 0;
}

static void save() {
  prefs.putInt("day", day);
  prefs.putUInt("ms", todayMs);
  prefs.putInt("count", todayCount);
  lastSave = millis();
}

static void checkDay() {
  int d = todayYmd();
  if (d == 0 || d == day) return;

  if (day != 0 && todayMs > 0) {
    char line[48];
    snprintf(line, sizeof(line), "%04d-%02d-%02d,%lu,%d", day / 10000, day / 100 % 100, day % 100,
             (unsigned long)(todayMs / 1000), todayCount);
    logAppend("/labterm/daily.csv", line, "date,sec,count");
    pushHistory(day, todayMs / 1000);
  }
  // 日付をまたいだら区間を切る
  bool running = segStart != 0;
  logSegment(false);
  if (running) segStart = time(nullptr);

  day = d;
  todayMs = 0;
  todayCount = 0;
  save();
}

static bool longBreak = false;

static uint32_t lengthOf(Phase p) {
  if (p == Phase::Work) return cfg.workMin * 60000UL;
  return (longBreak ? cfg.longBreakMin : cfg.breakMin) * 60000UL;
}

static void nextPhase() {
  if (phase == Phase::Work) {
    beep(3);
    todayCount++;
    logSegment(true);
    save();
    // 休憩はそのまま始める。n回ごとに長め
    // 長めの休憩はいらないので止めておく
    // longBreak = cfg.longEvery > 0 && todayCount % cfg.longEvery == 0;
    phase = Phase::Break;
    state = RunState::Running;
  } else {
    beep(2);
    phase = Phase::Work;
    state = RunState::Idle;
  }
  remainMs = lengthOf(phase);
}

void focusBegin() {
  remainMs = lengthOf(phase);

  prefs.begin("focus", false);
  day = prefs.getInt("day", 0);
  todayMs = prefs.getUInt("ms", 0);
  todayCount = prefs.getInt("count", 0);
  if (prefs.getBytes("hist", hist, sizeof(hist)) != sizeof(hist)) {
    memset(hist, 0, sizeof(hist));
  }
  checkDay();
}

void focusLoop() {
  checkDay();
  if (state != RunState::Running) return;
  uint32_t now = millis();
  uint32_t dt = now - lastTick;
  lastTick = now;

  if (phase == Phase::Work) {
    uint32_t add = min(dt, remainMs);
    todayMs += add;
    segMs += add;
    if (now - lastSave > 60000) save();
  }

  if (dt >= remainMs) {
    nextPhase();
  } else {
    remainMs -= dt;
  }
}

void focusStart() {
  if (state == RunState::Running) return;
  state = RunState::Running;
  lastTick = millis();
  if (phase == Phase::Work && timeValid()) segStart = time(nullptr);
}

void focusPause() {
  if (state != RunState::Running) return;
  focusLoop();
  state = RunState::Paused;
  logSegment(false);
  save();
}

void focusReset() {
  if (state == RunState::Running) focusLoop();
  logSegment(false);
  save();
  phase = Phase::Work;
  state = RunState::Idle;
  remainMs = lengthOf(phase);
}

void focusToggle() {
  if (state == RunState::Running) {
    focusPause();
  } else {
    focusStart();
  }
}

Phase focusPhase() { return phase; }
RunState focusState() { return state; }
uint32_t focusRemainMs() { return remainMs; }
uint32_t focusLengthMs() { return lengthOf(phase); }

uint32_t focusTodaySec() { return todayMs / 1000; }
int focusTodayCount() { return todayCount; }

void focusWeek(uint32_t out[7]) {
  for (int i = 0; i < 7; i++) out[i] = 0;
  out[6] = todayMs / 1000;
  if (day == 0) return;
  for (int i = 0; i < 6; i++) {
    int d = ymdAddDays(day, i - 6);
    for (const DayTotal& h : hist) {
      if (h.day == d) out[i] = h.sec;
    }
  }
}

bool focusLongBreak() { return phase == Phase::Break && longBreak; }

// "2026-10-21 11:24:28" (端末のタイムゾーン) → UNIX秒
static time_t parseLocal(const char* s) {
  struct tm t = {};
  if (sscanf(s, "%d-%d-%d %d:%d:%d", &t.tm_year, &t.tm_mon, &t.tm_mday, &t.tm_hour, &t.tm_min,
             &t.tm_sec) != 6) {
    return 0;
  }
  t.tm_year -= 1900;
  t.tm_mon -= 1;
  t.tm_isdst = -1;
  return mktime(&t);
}

void focusEach(time_t after, FocusSessionFn fn, void* arg) {
  File f = logFS().open("/labterm/focus.csv", FILE_READ);
  if (!f) return;
  char line[96];
  while (f.available()) {
    size_t n = f.readBytesUntil('\n', line, sizeof(line) - 1);
    line[n] = '\0';
    char* comma = strchr(line, ',');
    if (!comma) continue;
    *comma = '\0';
    char* rest = comma + 1;
    char* comma2 = strchr(rest, ',');
    if (!comma2) continue;
    *comma2 = '\0';
    time_t start = parseLocal(line);
    time_t end = parseLocal(rest);
    if (start == 0 || end == 0 || start <= after) continue;  // ヘッダや古いもの
    unsigned long sec = 0;
    int done = 0;
    if (sscanf(comma2 + 1, "%lu,%d", &sec, &done) != 2) continue;
    fn(start, end, sec, done != 0, arg);
  }
  f.close();
}
