#include "console.h"

#include <Arduino.h>

#include "config.h"
#include "focus.h"
#include "input.h"
#include "storage.h"
#include "timekeep.h"
#include "ui.h"

static char line[96];
static size_t lineLen = 0;
static bool lineOverflow = false;

static void printStatus() {
  char buf[24];
  fmtTime(time(nullptr), buf, sizeof(buf));
  Serial.printf("time  %s%s\n", buf, timeValid() ? "" : " (invalid)");
  Serial.printf("sd    %s\n", sdReady() ? "ok" : "none");
  if (lastNtpSync()) {
    fmtTime(lastNtpSync(), buf, sizeof(buf));
    Serial.printf("ntp   %s\n", buf);
  } else {
    Serial.println("ntp   -");
  }
  Serial.printf("focus %lus / %d\n", (unsigned long)focusTodaySec(), focusTodayCount());
  Serial.printf("heap  %u\n", (unsigned)ESP.getFreeHeap());
  Serial.println("OK");
}

// ログ(csv)だけ読めるようにする。config.jsonはパスワードが入ってるので不可
static bool isLogPath(const char* path) {
  size_t n = strlen(path);
  return strncmp(path, "/labterm/", 9) == 0 && !strstr(path, "..") && n > 4 &&
         strcmp(path + n - 4, ".csv") == 0;
}

static void catFile(const char* path) {
  if (!isLogPath(path)) {
    Serial.println("ERR bad path");
    return;
  }
  File f = logFS().open(path, FILE_READ);
  if (!f) {
    Serial.println("ERR not found");
    return;
  }
  while (f.available()) Serial.write(f.read());
  f.close();
  Serial.println("OK");
}

// "config <バイト数>" の後にjson本体が来る
static void receiveConfig(const char* arg) {
  char* end;
  long n = strtol(arg, &end, 10);
  if (*end != '\0' || n <= 0 || n > MAX_CONFIG_SIZE) {
    Serial.println("ERR bad size");
    return;
  }
  char* buf = (char*)malloc(n);
  if (!buf) {
    Serial.println("ERR no memory");
    return;
  }

  Serial.println("READY");
  size_t got = 0;
  uint32_t t0 = millis();
  while (got < (size_t)n && millis() - t0 < 5000) {
    int avail = Serial.available();
    if (avail > 0) {
      got += Serial.readBytes(buf + got, min((size_t)avail, (size_t)n - got));
    } else {
      delay(1);
    }
  }

  bool ok = got == (size_t)n && saveConfig(buf, n);
  memset(buf, 0, n);  // パスワード入りなので消しておく
  free(buf);

  if (ok) {
    timeResync();
    uiDirty();
    Serial.println("OK");
  } else {
    Serial.println(got == (size_t)n ? "ERR invalid config" : "ERR timeout");
  }
}

static void handle(const char* s) {
  if (strcmp(s, "status") == 0) {
    printStatus();
  } else if (strncmp(s, "cat ", 4) == 0) {
    catFile(s + 4);
  } else if (strncmp(s, "config ", 7) == 0) {
    receiveConfig(s + 7);
  } else if (strncmp(s, "time ", 5) == 0) {
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
  } else if (strncmp(s, "btn ", 4) == 0 || strncmp(s, "hold ", 5) == 0) {
    // 実機を触らずに動作確認する用
    bool hold = s[0] == 'h';
    int idx = s[hold ? 5 : 4] - 'a';
    if (idx < 0 || idx > 2) {
      Serial.println("ERR bad button");
      return;
    }
    inputButton(idx, hold);
    Serial.println("OK");
  } else if (strncmp(s, "tap ", 4) == 0) {
    int x, y;
    if (sscanf(s + 4, "%d %d", &x, &y) != 2) {
      Serial.println("ERR bad tap");
      return;
    }
    inputTap(x, y);
    Serial.println("OK");
  } else if (strncmp(s, "press ", 6) == 0) {
    int x, y;
    if (sscanf(s + 6, "%d %d", &x, &y) != 2) {
      Serial.println("ERR bad press");
      return;
    }
    inputLongPress(x, y);
    Serial.println("OK");
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
