#include "config.h"
#include <ArduinoJson.h>
#include <LittleFS.h>
#include <SD.h>

#include "storage.h"

Config cfg;

static int clampInt(JsonVariantConst v, int def, int lo, int hi) {
  if (!v.is<int>()) return def;
  return constrain(v.as<int>(), lo, hi);
}

static void copyStr(char* dst, size_t n, const char* src) {
  if (!src) src = "";
  strlcpy(dst, src, n);
}

// 文字の途中で切れないようにコピー
static void copyUtf8(char* dst, size_t n, const char* src) {
  if (!src) src = "";
  size_t len = strlen(src);
  if (len >= n) {
    len = n - 1;
    while (len > 0 && (src[len] & 0xC0) == 0x80) len--;
  }
  memcpy(dst, src, len);
  dst[len] = '\0';
}

// "2026-10-27" -> 20261027
static int parseYmd(const char* s) {
  int y, m, d;
  char tail;
  if (!s || sscanf(s, "%4d-%2d-%2d%c", &y, &m, &d, &tail) != 3) return 0;
  if (y < 2000 || y > 2100 || m < 1 || m > 12 || d < 1 || d > 31) return 0;
  return y * 10000 + m * 100 + d;
}

bool loadConfig() {
  // SDにあればそっちを優先
  File f;
  if (sdReady()) f = SD.open("/labterm/config.json", FILE_READ);
  if (!f) f = LittleFS.open("/config.json", FILE_READ);
  if (!f) {
    Serial.println("[cfg] no config.json");
    return false;
  }

  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, f);
  f.close();
  if (err) {
    Serial.printf("[cfg] parse error: %s\n", err.c_str());
    return false;
  }

  cfg.wifiCount = 0;
  for (JsonObject w : doc["wifi"].as<JsonArray>()) {
    if (cfg.wifiCount >= MAX_WIFI) break;
    WifiCred& c = cfg.wifi[cfg.wifiCount];
    copyStr(c.ssid, sizeof(c.ssid), w["ssid"]);
    copyStr(c.pass, sizeof(c.pass), w["pass"]);
    if (c.ssid[0]) cfg.wifiCount++;
  }
  if (doc["tz"].is<const char*>()) copyStr(cfg.tz, sizeof(cfg.tz), doc["tz"]);
  if (doc["ntp"].is<const char*>()) copyStr(cfg.ntp, sizeof(cfg.ntp), doc["ntp"]);

  JsonObjectConst pomo = doc["pomodoro"];
  cfg.workMin = clampInt(pomo["work"], cfg.workMin, 1, 180);
  cfg.breakMin = clampInt(pomo["break"], cfg.breakMin, 1, 60);

  cfg.deadlineCount = 0;
  for (JsonObjectConst d : doc["deadlines"].as<JsonArrayConst>()) {
    if (cfg.deadlineCount >= MAX_DEADLINES) break;
    Deadline& dl = cfg.deadlines[cfg.deadlineCount];
    dl.ymd = parseYmd(d["date"]);
    copyUtf8(dl.title, sizeof(dl.title), d["title"]);
    if (dl.ymd) cfg.deadlineCount++;
  }

  Serial.printf("[cfg] loaded, wifi=%d\n", cfg.wifiCount);
  return true;
}
