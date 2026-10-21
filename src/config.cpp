#include "config.h"
#include <ArduinoJson.h>
#include <LittleFS.h>
#include <SD.h>

#include "storage.h"

Config cfg;

static const char* SD_PATH = "/labterm/config.json";
static const char* FLASH_PATH = "/config.json";

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

// "13:30" -> 810
static int parseHm(const char* s) {
  int h, m;
  char tail;
  if (!s || sscanf(s, "%2d:%2d%c", &h, &m, &tail) != 2) return -1;
  if (h < 0 || h > 23 || m < 0 || m > 59) return -1;
  return h * 60 + m;
}

static int dowIndex(const char* s) {
  static const char* en[] = {"sun", "mon", "tue", "wed", "thu", "fri", "sat"};
  static const char* ja[] = {"日", "月", "火", "水", "木", "金", "土"};
  for (int i = 0; i < 7; i++) {
    if (strcasecmp(s, en[i]) == 0 || strcmp(s, ja[i]) == 0) return i;
  }
  return -1;
}

// "mon,wed" でも ["mon","wed"] でもいい
static uint8_t parseDows(JsonVariantConst v) {
  uint8_t bits = 0;
  if (v.is<JsonArrayConst>()) {
    for (JsonVariantConst e : v.as<JsonArrayConst>()) {
      int i = e.is<const char*>() ? dowIndex(e.as<const char*>()) : -1;
      if (i >= 0) bits |= 1 << i;
    }
  } else if (v.is<const char*>()) {
    char buf[64];
    strlcpy(buf, v.as<const char*>(), sizeof(buf));
    char* save = nullptr;
    for (char* tok = strtok_r(buf, ", ", &save); tok; tok = strtok_r(nullptr, ", ", &save)) {
      int i = dowIndex(tok);
      if (i >= 0) bits |= 1 << i;
    }
  }
  return bits;
}

static bool parseItem(JsonObjectConst o, Item& it) {
  it.ymd = o["date"].is<const char*>() ? parseYmd(o["date"]) : 0;
  it.dows = it.ymd ? 0 : parseDows(o["dow"]);
  it.start = parseHm(o["start"]);
  it.end = parseHm(o["end"]);
  it.mtg = o["mtg"] | false;
  copyUtf8(it.title, sizeof(it.title), o["title"]);
  if (it.end >= 0 && it.end <= it.start) it.end = -1;
  return it.start >= 0 && (it.ymd || it.dows) && it.title[0];
}

bool loadConfig() {
  // SDにあればそっちを優先
  File f;
  if (sdReady()) f = SD.open(SD_PATH, FILE_READ);
  if (!f) f = LittleFS.open(FLASH_PATH, FILE_READ);
  if (!f) {
    Serial.println("[cfg] no config.json");
    return false;
  }
  if (f.size() > MAX_CONFIG_SIZE) {
    Serial.println("[cfg] config.json too large");
    f.close();
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
  cfg.remindMin = clampInt(doc["remind_min"], cfg.remindMin, 0, 60);
  cfg.pauseOnMtg = doc["pause_on_mtg"] | cfg.pauseOnMtg;
  cfg.volume = clampInt(doc["volume"], cfg.volume, 0, 255);
  if (doc["clock"].is<const char*>()) cfg.clockAnalog = strcmp(doc["clock"], "analog") == 0;
  if (doc["focus_style"].is<const char*>()) {
    static const char* styles[] = {"normal", "ring", "dial", "water", "grid", "hourglass", "eva"};
    for (int i = 0; i < 7; i++) {
      if (strcmp(doc["focus_style"], styles[i]) == 0) cfg.focusStyle = i;
    }
  }

  JsonObjectConst br = doc["brightness"];
  cfg.brightDay = clampInt(br["day"], cfg.brightDay, 1, 255);
  cfg.brightNight = clampInt(br["night"], cfg.brightNight, 0, 255);
  if (parseHm(br["night_from"]) >= 0) cfg.nightFrom = parseHm(br["night_from"]);
  if (parseHm(br["night_to"]) >= 0) cfg.nightTo = parseHm(br["night_to"]);

  cfg.deadlineCount = 0;
  for (JsonObjectConst d : doc["deadlines"].as<JsonArrayConst>()) {
    if (cfg.deadlineCount >= MAX_DEADLINES) break;
    Deadline& dl = cfg.deadlines[cfg.deadlineCount];
    dl.ymd = parseYmd(d["date"]);
    copyUtf8(dl.title, sizeof(dl.title), d["title"]);
    if (dl.ymd) cfg.deadlineCount++;
  }

  cfg.itemCount = 0;
  for (JsonObjectConst o : doc["schedule"].as<JsonArrayConst>()) {
    if (cfg.itemCount >= MAX_ITEMS) break;
    if (parseItem(o, cfg.items[cfg.itemCount])) {
      cfg.itemCount++;
    } else {
      Serial.println("[cfg] skip bad schedule item");
    }
  }

  // QRにするのはhttp(s)のリンクだけ
  JsonObjectConst card = doc["card"];
  copyUtf8(cfg.cardName, sizeof(cfg.cardName), card["name"]);
  cfg.linkCount = 0;
  for (JsonObjectConst l : card["links"].as<JsonArrayConst>()) {
    if (cfg.linkCount >= MAX_LINKS) break;
    const char* url = l["url"];
    if (!url || strlen(url) >= sizeof(Link::url) ||
        (strncmp(url, "https://", 8) != 0 && strncmp(url, "http://", 7) != 0)) {
      Serial.println("[cfg] skip bad link");
      continue;
    }
    Link& dst = cfg.links[cfg.linkCount++];
    copyUtf8(dst.label, sizeof(dst.label), l["label"]);
    strlcpy(dst.url, url, sizeof(dst.url));
  }

  Serial.printf("[cfg] loaded, wifi=%d items=%d\n", cfg.wifiCount, cfg.itemCount);
  return true;
}

bool saveConfig(const char* json, size_t len) {
  if (len == 0 || len > MAX_CONFIG_SIZE) return false;
  JsonDocument doc;
  if (deserializeJson(doc, json, len)) return false;
  if (!doc.is<JsonObject>()) return false;

  // 一旦別名で書いてから置き換える
  fs::FS& fs = sdReady() ? (fs::FS&)SD : (fs::FS&)LittleFS;
  const char* path = sdReady() ? SD_PATH : FLASH_PATH;
  String tmp = String(path) + ".tmp";
  File f = fs.open(tmp, FILE_WRITE);
  if (!f) return false;
  size_t written = f.write((const uint8_t*)json, len);
  f.close();
  if (written != len) {
    fs.remove(tmp);
    return false;
  }
  fs.remove(path);
  if (!fs.rename(tmp, path)) return false;

  Serial.println("[cfg] saved");
  return loadConfig();
}
