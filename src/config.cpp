#include "config.h"
#include <ArduinoJson.h>
#include <LittleFS.h>

Config cfg;

static int clampInt(JsonVariantConst v, int def, int lo, int hi) {
  if (!v.is<int>()) return def;
  return constrain(v.as<int>(), lo, hi);
}

static void copyStr(char* dst, size_t n, const char* src) {
  if (!src) src = "";
  strlcpy(dst, src, n);
}

bool loadConfig() {
  if (!LittleFS.begin(true)) {
    Serial.println("[cfg] littlefs mount failed");
    return false;
  }
  File f = LittleFS.open("/config.json", "r");
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

  Serial.printf("[cfg] loaded, wifi=%d\n", cfg.wifiCount);
  return true;
}
