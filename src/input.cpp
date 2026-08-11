#include "input.h"

#include <M5Unified.h>

#include "focus.h"
#include "presence.h"
#include "ui.h"

void inputBegin() {
  // タッチの機種は画面下をA/B/Cボタンとして使う
  if (M5.Touch.isEnabled()) {
    M5.setTouchButtonHeight(UI_FOOTER_H);
  }
}

// 研究を始めたら自動で入室にする
static void startFocus() {
  if (focusPhase() == Phase::Work && presenceGet() != Presence::In) {
    presenceSet(Presence::In);
    uiToast("入室しました");
  }
  focusStart();
}

void inputButton(int idx, bool hold) {
  if (hold) {
    if (idx == 1) uiNextScreen();
    return;
  }
  if (uiScreen() == Screen::Presence) {
    static const Presence ps[] = {Presence::In, Presence::Away, Presence::Out};
    static const char* msg[] = {"入室しました", "外出します", "おつかれさまでした"};
    if (presenceGet() != ps[idx]) {
      presenceSet(ps[idx]);
      uiToast(msg[idx]);
    }
  } else {
    switch (idx) {
      case 0: startFocus(); break;
      case 1: focusPause(); break;
      case 2: focusReset(); break;
    }
  }
  uiDirty();
}

void inputTap(int x, int y) {
  if (y >= UI_FOOTER_Y) return;
  if (y < UI_HEADER_H) {
    uiSetScreen((Screen)(x / UI_TAB_W));
    return;
  }
  if (uiScreen() == Screen::Focus) {
    if (focusState() == RunState::Running) {
      focusPause();
    } else {
      startFocus();
    }
    uiDirty();
  }
}

void inputFlick(int dir) {
  uiMoveScreen(dir);
}

void inputLoop() {
  m5::Button_Class* btns[] = {&M5.BtnA, &M5.BtnB, &M5.BtnC};
  for (int i = 0; i < 3; i++) {
    if (btns[i]->wasClicked()) inputButton(i, false);
    if (btns[i]->wasHold()) inputButton(i, true);
  }

  for (int i = 0; i < M5.Touch.getCount(); i++) {
    auto t = M5.Touch.getDetail(i);
    if (t.base_y >= UI_FOOTER_Y) continue;  // ボタン側で処理済み
    if (t.wasClicked()) {
      inputTap(t.x, t.y);
    } else if (t.wasFlicked() && abs(t.distanceX()) > 40) {
      inputFlick(t.distanceX() < 0 ? 1 : -1);
    }
  }
}
