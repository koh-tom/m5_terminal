#include "beep.h"

#include <M5Unified.h>

#include "config.h"

void beep(int times, int freq) {
  if (cfg.volume == 0) return;
  M5.Speaker.setVolume(cfg.volume);
  for (int i = 0; i < times; i++) {
    M5.Speaker.tone(freq, 120);
    delay(200);
  }
}
