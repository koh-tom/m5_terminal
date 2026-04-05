#include "focus.h"

#include <M5Unified.h>

#include "config.h"

static Phase phase = Phase::Work;
static RunState state = RunState::Idle;
static uint32_t remainMs = 0;
static uint32_t lastTick = 0;

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
}

void focusLoop() {
  if (state != RunState::Running) return;
  uint32_t now = millis();
  uint32_t dt = now - lastTick;
  lastTick = now;

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
}

void focusReset() {
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
