#include <M5Unified.h>

#include "config.h"
#include "console.h"
#include "focus.h"
#include "fonts.h"
#include "input.h"
#include "light.h"
#include "presence.h"
#include "schedule.h"
#include "storage.h"
#include "tilt.h"
#include "timekeep.h"
#include "ui.h"

// 大きいvlwフォントはグリフをスタックに展開するので広めにとる
SET_LOOP_TASK_STACK_SIZE(32 * 1024);

void setup() {
  // config受信用に大きめにしておく (begin前に呼ぶ)
  Serial.setRxBufferSize(4096);

  auto mc = M5.config();
  mc.serial_baudrate = 115200;
  M5.begin(mc);

  storageBegin();
  loadConfig();
  timeBegin();
  fontsBegin();
  focusBegin();
  presenceBegin();
  inputBegin();
  tiltBegin();
  lightBegin();
  uiBegin();
}

void loop() {
  M5.update();
  consoleLoop();
  timeLoop();
  inputLoop();
  tiltLoop();
  lightLoop();

  // 手をかざしたら明るくする
  static bool hover = false;
  if (lightHover() && !hover) uiWake();
  hover = lightHover();
  focusLoop();
  presenceLoop();
  reminderLoop();
  uiLoop();
  delay(10);
}
