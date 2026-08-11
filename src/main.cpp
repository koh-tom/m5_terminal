#include <M5Unified.h>

#include "config.h"
#include "console.h"
#include "focus.h"
#include "input.h"
#include "presence.h"
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
  presenceBegin();
  inputBegin();
  uiBegin();
}

void loop() {
  M5.update();
  consoleLoop();
  timeLoop();
  inputLoop();
  focusLoop();
  presenceLoop();
  reminderLoop();
  uiLoop();
  delay(10);
}
