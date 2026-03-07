#include <M5Unified.h>

void setup() {
  M5.begin();
  M5.Display.setTextSize(2);
  M5.Display.drawString("labterm", 10, 10);
}

void loop() {
  M5.update();
  delay(10);
}
