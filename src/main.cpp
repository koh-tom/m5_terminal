#include <M5Unified.h>

#include "config.h"
#include "console.h"
#include "focus.h"
#include "input.h"
#include "schedule.h"
#include "storage.h"
#include "timekeep.h"
#include "ui.h"

void setup() {
  auto mc = M5.config();
  mc.serial_baudrate = 115200;
  M5.begin(mc);

  storageBegin();
  loadConfig();
  timeBegin();
  focusBegin();
  inputBegin();
  uiBegin();
}

void loop() {
  M5.update();
  consoleLoop();
  timeLoop();
  inputLoop();
  focusLoop();
  reminderLoop();
  uiLoop();
  delay(10);
}
