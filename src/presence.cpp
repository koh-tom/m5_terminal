#include "presence.h"

#include <Arduino.h>
#include <Preferences.h>
#include <esp_random.h>

#include "config.h"
#include "storage.h"
#include "timekeep.h"

static const char* LOG = "/labterm/presence.csv";
static const char* LOG_V1 = "/labterm/presence_v1.csv";
static const char* HEADER = "gen,seq,epoch,time,kind,state,voids,source";
static constexpr uint32_t UNDO_MS = 5000;

static Preferences prefs;
static Presence state = Presence::Out;
static time_t since = 0;
static time_t arrived = 0;
static uint32_t inSec = 0;
static int day = 0;
static uint32_t gen = 0;
static uint32_t seq = 0;

// 取消用に直前の状態を覚えておく
static struct {
  bool valid;
  uint32_t at;  // millis
  uint32_t seq;
  Presence to;
  Presence state;
  time_t since, arrived;
  uint32_t inSec;
} last;

static const char* NAMES[] = {"out", "in", "away"};
static const char* SOURCES[] = {"button", "focus"};

static void save() {
  prefs.putUChar("state", (uint8_t)state);
  prefs.putLong64("since", since);
  prefs.putLong64("arrived", arrived);
  prefs.putUInt("in_sec", inSec);
  prefs.putInt("day", day);
  prefs.putUInt("seq", seq);
}

static void checkDay() {
  int d = todayYmd();
  if (d == 0 || d == day) return;
  time_t now = time(nullptr);
  // 日付が変わったら数え直し (在室中ならそのまま続ける)
  inSec = 0;
  arrived = 0;
  if (state == Presence::In) {
    since = now;
    arrived = now;
  }
  day = d;
  save();
}

// 旧形式 (time,state) のログは名前を変えて残す
static void migrateLog() {
  fs::FS& fs = logFS();
  File f = fs.open(LOG, FILE_READ);
  if (!f) return;
  String head = f.readStringUntil('\n');
  f.close();
  head.trim();
  if (head == "time,state") {
    fs.remove(LOG_V1);
    fs.rename(LOG, LOG_V1);
    Serial.println("[presence] old log -> presence_v1.csv");
  }
}

// "2026-10-22T09:18:31+09:00"
static void isoTime(time_t t, char* buf, size_t n) {
  struct tm tm;
  localtime_r(&t, &tm);
  char z[8];
  strftime(buf, n, "%Y-%m-%dT%H:%M:%S", &tm);
  strftime(z, sizeof(z), "%z", &tm);  // +0900
  size_t len = strlen(buf);
  snprintf(buf + len, n - len, "%.3s:%.2s", z, z + 3);
}

// ログに1行書いて、PCに通知する
static void record(const char* kind, Presence p, uint32_t voids, PresenceSource src, time_t now) {
  seq++;
  prefs.putUInt("seq", seq);  // 先に保存 (途中で落ちても番号を使い回さない)
  char ts[32], line[128];
  isoTime(now, ts, sizeof(ts));
  char v[12] = "";
  if (voids) snprintf(v, sizeof(v), "%lu", (unsigned long)voids);
  snprintf(line, sizeof(line), "%lu,%lu,%lld,%s,%s,%s,%s,%s", (unsigned long)gen, (unsigned long)seq,
           (long long)now, ts, kind, voids ? "" : NAMES[(int)p], v, SOURCES[(int)src]);
  logAppend(LOG, line, HEADER);

  if (voids) {
    Serial.printf("@labterm {\"type\":\"event\",\"gen\":%lu,\"seq\":%lu,\"kind\":\"void\",\"voids\":%lu,"
                  "\"epoch\":%lld,\"source\":\"%s\"}\n",
                  (unsigned long)gen, (unsigned long)seq, (unsigned long)voids, (long long)now,
                  SOURCES[(int)src]);
  } else {
    Serial.printf("@labterm {\"type\":\"event\",\"gen\":%lu,\"seq\":%lu,\"kind\":\"state\",\"state\":\"%s\","
                  "\"epoch\":%lld,\"source\":\"%s\"}\n",
                  (unsigned long)gen, (unsigned long)seq, NAMES[(int)p], (long long)now, SOURCES[(int)src]);
  }
}

void presenceBegin() {
  prefs.begin("presence", false);
  state = (Presence)prefs.getUChar("state", 0);
  if (state > Presence::Away) state = Presence::Out;
  since = prefs.getLong64("since", 0);
  arrived = prefs.getLong64("arrived", 0);
  inSec = prefs.getUInt("in_sec", 0);
  day = prefs.getInt("day", 0);
  seq = prefs.getUInt("seq", 0);

  // genはNVSを初期化するたびに変わる (PC側でseqの振り直しを見分ける)
  gen = prefs.getUInt("gen", 0);
  if (gen == 0) {
    do { gen = esp_random(); } while (gen == 0);
    seq = 0;
    prefs.putUInt("gen", gen);
    prefs.putUInt("seq", seq);
  }

  migrateLog();
  checkDay();
}

void presenceLoop() {
  static uint32_t lastCheck = 0;
  if (last.valid && millis() - last.at >= UNDO_MS) last.valid = false;
  if (millis() - lastCheck < 1000) return;
  lastCheck = millis();
  checkDay();
}

bool presenceCanSet(Presence p) {
  return !(state == Presence::Out && p == Presence::Away);
}

bool presenceSet(Presence p, PresenceSource src) {
  if (p == state) return true;
  if (!timeValid() || !presenceCanSet(p)) return false;
  time_t now = time(nullptr);

  last = {true, millis(), 0, p, state, since, arrived, inSec};

  if (state == Presence::In) inSec += now - since;
  if (p == Presence::In && arrived == 0) arrived = now;
  state = p;
  since = now;

  record("state", p, 0, src, now);
  last.seq = seq;
  save();
  return true;
}

bool presenceUndo() {
  if (!last.valid || millis() - last.at >= UNDO_MS) return false;
  last.valid = false;
  state = last.state;
  since = last.since;
  arrived = last.arrived;
  inSec = last.inSec;
  record("void", state, last.seq, PresenceSource::Button, time(nullptr));
  save();
  return true;
}

uint32_t presenceUndoLeftMs() {
  if (!last.valid) return 0;
  uint32_t e = millis() - last.at;
  return e < UNDO_MS ? UNDO_MS - e : 0;
}

Presence presenceUndoTarget() { return last.to; }

Presence presenceGet() { return state; }
time_t presenceSince() { return since; }
time_t presenceArrived() { return arrived; }

uint32_t presenceTodaySec() {
  uint32_t s = inSec;
  if (state == Presence::In) s += time(nullptr) - since;
  return s;
}

uint32_t presenceGen() { return gen; }
uint32_t presenceSeq() { return seq; }
const char* presenceName(Presence p) { return NAMES[(int)p]; }

void presenceEach(uint32_t g, uint32_t afterSeq, PresenceEventFn fn, void* arg) {
  File f = logFS().open(LOG, FILE_READ);
  if (!f) return;
  char line[160];
  while (f.available()) {
    size_t n = f.readBytesUntil('\n', line, sizeof(line) - 1);
    line[n] = '\0';
    if (n && line[n - 1] == '\r') line[n - 1] = '\0';
    char* end;
    unsigned long lg = strtoul(line, &end, 10);
    if (end == line || *end != ',') continue;  // ヘッダなど
    unsigned long ls = strtoul(end + 1, nullptr, 10);
    if (lg == g && ls > afterSeq) fn(line, arg);
  }
  f.close();
}
