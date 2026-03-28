#include "input.h"

#include <M5Unified.h>

#include "focus.h"
#include "ui.h"

void inputBegin() {
  // タッチの機種は画面下をA/B/Cボタンとして使う
  if (M5.Touch.isEnabled()) {
    M5.setTouchButtonHeight(UI_FOOTER_H);
  }
}

void inputButton(int idx, bool hold) {
  if (hold) {
    if (idx == 1) uiNextScreen();
    return;
  }
  switch (idx) {
    case 0: focusStart(); break;
    case 1: focusPause(); break;
    case 2: focusReset(); break;
  }
  uiDirty();
}

void inputTap(int x, int y) {
  (void)x;
  if (y >= UI_FOOTER_Y) return;
  if (uiScreen() == Screen::Focus) {
    focusToggle();
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
