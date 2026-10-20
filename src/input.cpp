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
  if (focusPhase() == Phase::Work && presenceGet() != Presence::In &&
      presenceSet(Presence::In)) {
    uiToast("入室しました");
  }
  focusStart();
}

void inputButton(int idx, bool hold) {
  uiWake();
  if (hold) {
    if (idx == 1) uiNextScreen();
    if (idx == 2) uiToggleStyle();
    return;
  }

  // ボタンの無い画面ではA/Cで画面を送る (Basic用)
  if (!uiHasFooter()) {
    if (idx == 0) uiMoveScreen(-1);
    if (idx == 2) uiMoveScreen(1);
    return;
  }

  if (uiScreen() == Screen::Presence) {
    static const Presence ps[] = {Presence::In, Presence::Away, Presence::Out};
    static const char* msg[] = {"入室しました", "外出します", "おつかれさまでした"};
    if (presenceGet() != ps[idx]) {
      uiToast(presenceSet(ps[idx]) ? msg[idx] : "時刻が未設定です");
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
  uiWake();
  if (uiHasFooter() && y >= UI_FOOTER_Y) return;
  if (y < UI_HEADER_H) {
    if (x >= 0) uiSetScreen((Screen)(x / UI_TAB_W));
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

// 長押しで表示を切り替え
void inputLongPress(int x, int y) {
  (void)x;
  uiWake();
  if (y < UI_HEADER_H || (uiHasFooter() && y >= UI_FOOTER_Y)) return;
  uiToggleStyle();
}

void inputFlick(int dir) {
  uiWake();
  uiMoveScreen(dir);
}

void inputLoop() {
  // タッチの機種で下にボタンが無い画面なら、ボタンとしては扱わない
  bool touch = M5.Touch.isEnabled();
  bool buttons = !touch || uiHasFooter();

  if (buttons) {
    m5::Button_Class* btns[] = {&M5.BtnA, &M5.BtnB, &M5.BtnC};
    for (int i = 0; i < 3; i++) {
      if (btns[i]->wasClicked()) inputButton(i, false);
      if (btns[i]->wasHold()) inputButton(i, true);
    }
  }

  for (int i = 0; i < M5.Touch.getCount(); i++) {
    auto t = M5.Touch.getDetail(i);
    if (buttons && t.base_y >= UI_FOOTER_Y) continue;  // ボタン側で処理済み
    if (t.wasClicked()) {
      inputTap(t.x, t.y);
    } else if (t.wasHold()) {
      inputLongPress(t.x, t.y);
    } else if (t.wasFlicked() && abs(t.distanceX()) > 40) {
      inputFlick(t.distanceX() < 0 ? 1 : -1);
    }
  }
}
