#include <M5Unified.h>

#include "config.h"
#include "console.h"
#include "focus.h"
#include "timekeep.h"
#include "ui.h"

static void handleButtons() {
  if (M5.BtnA.wasClicked()) focusStart();
  if (M5.BtnB.wasClicked()) focusPause();
  if (M5.BtnC.wasClicked()) focusReset();
  if (M5.BtnB.wasHold()) uiNextScreen();

  if (M5.BtnA.wasChangePressed() || M5.BtnB.wasChangePressed() || M5.BtnC.wasChangePressed()) {
    uiDirty();
  }
}

void setup() {
  auto mc = M5.config();
  mc.serial_baudrate = 115200;
  M5.begin(mc);

  loadConfig();
  timeBegin();
  focusBegin();
  uiBegin();
}

void loop() {
  M5.update();
  consoleLoop();
  timeLoop();
  handleButtons();
  focusLoop();
  uiLoop();
  delay(10);
}
