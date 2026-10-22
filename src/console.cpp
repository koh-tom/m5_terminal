#include "console.h"

#include <Arduino.h>

#ifndef LABTERM_VERSION
#define LABTERM_VERSION "dev"
#endif

#include "config.h"
#include "focus.h"
#include "input.h"
#include "light.h"
#include "pixelart.h"
#include "presence.h"
#include "storage.h"
#include "tilt.h"
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
  {
    struct tm t;
    time_t since = presenceSince();
    localtime_r(&since, &t);
    Serial.printf("presence %s since %02d:%02d (seq %lu)\n", presenceName(presenceGet()), t.tm_hour,
                  t.tm_min, (unsigned long)presenceSeq());
  }
  Serial.printf("focus %lus / %d\n", (unsigned long)focusTodaySec(), focusTodayCount());
  float x, y, z;
  tiltAccel(&x, &y, &z);
  Serial.printf("accel %.2f %.2f %.2f%s\n", x, y, z, tiltUpsideDown() ? " (upside down)" : "");
  if (lightAvailable()) Serial.printf("light %.2f lux / prox %d\n", lightLux(), lightProximity());
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

// "config <バイト数>" / "image <バイト数>" の後に本体が来る
static void receive(const char* arg, size_t maxSize, bool (*save)(const uint8_t*, size_t)) {
  char* end;
  long n = strtol(arg, &end, 10);
  if (*end != '\0' || n <= 0 || (size_t)n > maxSize) {
    Serial.println("ERR bad size");
    return;
  }
  uint8_t* buf = (uint8_t*)ps_malloc(n);
  if (!buf) buf = (uint8_t*)malloc(n);
  if (!buf) {
    Serial.println("ERR no memory");
    return;
  }

  Serial.println("READY");
  size_t got = 0;
  uint32_t t0 = millis();
  while (got < (size_t)n && millis() - t0 < 10000) {
    int avail = Serial.available();
    if (avail > 0) {
      got += Serial.readBytes(buf + got, min((size_t)avail, (size_t)n - got));
    } else {
      delay(1);
    }
  }

  bool ok = got == (size_t)n && save(buf, n);
  memset(buf, 0, n);  // configはパスワード入りなので消しておく
  free(buf);

  if (ok) {
    uiDirty();
    Serial.println("OK");
  } else {
    Serial.println(got == (size_t)n ? "ERR invalid data" : "ERR timeout");
  }
}

static bool saveConfigData(const uint8_t* d, size_t n) {
  if (!saveConfig((const char*)d, n)) return false;
  timeResync();
  return true;
}


// ---- PC (labterm-presence) 向けの機械用コマンド ----
//   要求: "@q <reqId> <command> [args]"   応答: "@labterm {json}"

static void machineError(unsigned long re, const char* code) {
  Serial.printf("@labterm {\"type\":\"error\",\"re\":%lu,\"code\":\"%s\"}\n", re, code);
}

static void machineHello(unsigned long re) {
  uint64_t mac = ESP.getEfuseMac();
  char id[18];
  snprintf(id, sizeof(id), "%02x:%02x:%02x:%02x:%02x:%02x", (int)(mac & 0xFF), (int)(mac >> 8 & 0xFF),
           (int)(mac >> 16 & 0xFF), (int)(mac >> 24 & 0xFF), (int)(mac >> 32 & 0xFF), (int)(mac >> 40 & 0xFF));
  Serial.printf("@labterm {\"type\":\"hello\",\"re\":%lu,\"proto\":1,\"device\":\"%s\",\"gen\":%lu,"
                "\"lastSeq\":%lu,\"fw\":\"%s\",\"epoch\":%lld,\"timeValid\":%s,\"uptime\":%lu}\n",
                re, id, (unsigned long)presenceGen(), (unsigned long)presenceSeq(), LABTERM_VERSION,
                (long long)time(nullptr), timeValid() ? "true" : "false", (unsigned long)(millis() / 1000));
}

struct SinceCtx {
  unsigned long re;
  int count;
};

// "gen,seq,epoch,time,kind,state,voids,source" を1件のeventにする
static void sinceLine(const char* line, void* arg) {
  SinceCtx* ctx = (SinceCtx*)arg;
  const char* f[8];
  char buf[160];
  strlcpy(buf, line, sizeof(buf));
  int n = 0;
  char* p = buf;
  f[n++] = p;
  while (*p && n < 8) {
    if (*p == ',') {
      *p = '\0';
      f[n++] = p + 1;
    }
    p++;
  }
  if (n < 8) return;
  bool isVoid = strcmp(f[4], "void") == 0;
  if (isVoid) {
    Serial.printf("@labterm {\"type\":\"event\",\"re\":%lu,\"gen\":%s,\"seq\":%s,\"kind\":\"void\","
                  "\"voids\":%s,\"epoch\":%s,\"source\":\"%s\"}\n",
                  ctx->re, f[0], f[1], f[6], f[2], f[7]);
  } else {
    Serial.printf("@labterm {\"type\":\"event\",\"re\":%lu,\"gen\":%s,\"seq\":%s,\"kind\":\"state\","
                  "\"state\":\"%s\",\"epoch\":%s,\"source\":\"%s\"}\n",
                  ctx->re, f[0], f[1], f[5], f[2], f[7]);
  }
  ctx->count++;
}

static void handleMachine(const char* s) {
  char* end;
  unsigned long re = strtoul(s, &end, 10);
  if (end == s || *end != ' ' || re == 0 || re > 99999999UL) return;  // 番号が無いものは無視
  const char* cmd = end + 1;

  if (strcmp(cmd, "hello") == 0) {
    machineHello(re);
  } else if (strcmp(cmd, "state") == 0) {
    Serial.printf("@labterm {\"type\":\"state\",\"re\":%lu,\"state\":\"%s\",\"since\":%lld,\"seq\":%lu}\n",
                  re, presenceName(presenceGet()), (long long)presenceSince(), (unsigned long)presenceSeq());
  } else if (strncmp(cmd, "since ", 6) == 0) {
    unsigned long g, sq;
    char tail;
    if (sscanf(cmd + 6, "%lu %lu%c", &g, &sq, &tail) != 2) {
      machineError(re, "bad_args");
      return;
    }
    SinceCtx ctx = {re, 0};
    presenceEach(g, sq, sinceLine, &ctx);
    Serial.printf("@labterm {\"type\":\"end\",\"re\":%lu,\"count\":%d}\n", re, ctx.count);
  } else if (strncmp(cmd, "time ", 5) == 0) {
    long long v = strtoll(cmd + 5, &end, 10);
    if (*end != '\0' || v < 1700000000LL || v > 4102444800LL) {
      machineError(re, "bad_args");
      return;
    }
    timeSet((time_t)v);
    Serial.printf("@labterm {\"type\":\"ok\",\"re\":%lu}\n", re);
  } else {
    machineError(re, "unknown");
  }
}

static void handle(const char* s) {
  if (strncmp(s, "@q ", 3) == 0) {
    handleMachine(s + 3);
    return;
  }
  if (strcmp(s, "status") == 0) {
    printStatus();
  } else if (strncmp(s, "cat ", 4) == 0) {
    catFile(s + 4);
  } else if (strncmp(s, "config ", 7) == 0) {
    receive(s + 7, MAX_CONFIG_SIZE, saveConfigData);
  } else if (strncmp(s, "art ", 4) == 0) {
    uiArtForce(strcmp(s + 4, "left") == 0 ? 1 : strcmp(s + 4, "right") == 0 ? 2 : 0);
    Serial.println("OK");
  } else if (strcmp(s, "image clear") == 0) {
    artClear();
    Serial.println("OK");
  } else if (strncmp(s, "image ", 6) == 0) {
    receive(s + 6, 8 + ART_MAX * ART_MAX * 2, artSave);
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
  } else if (strcmp(s, "card on") == 0 || strcmp(s, "card off") == 0) {
    uiCardForce(s[6] == 'n');
    Serial.println("OK");
  } else if (strcmp(s, "card next") == 0) {
    uiCardNext();
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
