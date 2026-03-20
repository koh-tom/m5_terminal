#include "console.h"

#include <Arduino.h>

#include "timekeep.h"
#include "ui.h"

static char line[96];
static size_t lineLen = 0;
static bool lineOverflow = false;

static void handle(const char* s) {
  if (strncmp(s, "time ", 5) == 0) {
    char* end;
    long long v = strtoll(s + 5, &end, 10);
    if (*end != '\0' || v < 1700000000LL || v > 4102444800LL) {
      Serial.println("ERR bad time");
      return;
    }
    timeSet((time_t)v);
    Serial.println("OK");
  } else if (strcmp(s, "shot") == 0) {
    uiShot();
  } else {
    Serial.println("ERR unknown");
  }
}

void consoleLoop() {
  while (Serial.available()) {
    int c = Serial.read();
    if (c == '\r') continue;
    if (c == '\n') {
      line[lineLen] = '\0';
      if (!lineOverflow && lineLen > 0) handle(line);
      lineLen = 0;
      lineOverflow = false;
    } else if (lineLen < sizeof(line) - 1) {
      line[lineLen++] = (char)c;
    } else {
      lineOverflow = true;
    }
  }
}
