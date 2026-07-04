#include "beep.h"

#include <M5Unified.h>

void beep(int times, int freq) {
  for (int i = 0; i < times; i++) {
    M5.Speaker.tone(freq, 120);
    delay(200);
  }
}
