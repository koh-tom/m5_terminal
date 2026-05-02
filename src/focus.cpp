#include "focus.h"

#include <M5Unified.h>
#include <Preferences.h>

#include "config.h"
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

static void save() {
  prefs.putInt("day", day);
  prefs.putUInt("ms", todayMs);
  prefs.putInt("count", todayCount);
  lastSave = millis();
}

static void checkDay() {
  int d = todayYmd();
  if (d == 0 || d == day) return;
  day = d;
  todayMs = 0;
  todayCount = 0;
  save();
}

static uint32_t lengthOf(Phase p) {
  return (p == Phase::Work ? cfg.workMin : cfg.breakMin) * 60000UL;
}

static void beep(int n) {
  for (int i = 0; i < n; i++) {
    M5.Speaker.tone(2000, 120);
    delay(200);
  }
}

static void nextPhase() {
  if (phase == Phase::Work) {
    beep(3);
    todayCount++;
    save();
    // 休憩はそのまま始める
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
  checkDay();
}

void focusLoop() {
  checkDay();
  if (state != RunState::Running) return;
  uint32_t now = millis();
  uint32_t dt = now - lastTick;
  lastTick = now;

  if (phase == Phase::Work) {
    todayMs += min(dt, remainMs);
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
}

void focusPause() {
  if (state != RunState::Running) return;
  focusLoop();
  state = RunState::Paused;
  save();
}

void focusReset() {
  if (state == RunState::Running) focusLoop();
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
