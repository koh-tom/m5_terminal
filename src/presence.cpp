#include "presence.h"

#include <Arduino.h>
#include <Preferences.h>

#include "config.h"
#include "storage.h"
#include "timekeep.h"

static Preferences prefs;
static Presence state = Presence::Out;
static time_t since = 0;
static time_t arrived = 0;
static uint32_t inSec = 0;
static int day = 0;

static const char* NAMES[] = {"out", "in", "away"};

static void save() {
  prefs.putUChar("state", (uint8_t)state);
  prefs.putLong64("since", since);
  prefs.putLong64("arrived", arrived);
  prefs.putUInt("in_sec", inSec);
  prefs.putInt("day", day);
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

void presenceBegin() {
  prefs.begin("presence", false);
  state = (Presence)prefs.getUChar("state", 0);
  if (state > Presence::Away) state = Presence::Out;
  since = prefs.getLong64("since", 0);
  arrived = prefs.getLong64("arrived", 0);
  inSec = prefs.getUInt("in_sec", 0);
  day = prefs.getInt("day", 0);
  checkDay();
}

// 退室し忘れ対策。直近の締め時刻より前から入室/外出のままなら退室にする
static void autoOut() {
  if (state == Presence::Out || cfg.autoOutMin < 0 || !timeValid()) return;
  time_t now = time(nullptr);
  struct tm t;
  localtime_r(&now, &t);
  t.tm_hour = cfg.autoOutMin / 60;
  t.tm_min = cfg.autoOutMin % 60;
  t.tm_sec = 0;
  time_t cut = mktime(&t);
  if (cut > now) cut -= 24 * 3600;
  if (since < cut) {
    Serial.println("[presence] auto out");
    presenceSet(Presence::Out);
  }
}

void presenceLoop() {
  static uint32_t last = 0;
  if (millis() - last < 1000) return;
  last = millis();
  checkDay();
  autoOut();
}

bool presenceSet(Presence p) {
  if (p == state) return true;
  if (!timeValid()) return false;
  time_t now = time(nullptr);
  if (state == Presence::In) inSec += now - since;
  if (p == Presence::In && arrived == 0) arrived = now;
  state = p;
  since = now;
  save();

  char ts[24], line[48];
  fmtTime(now, ts, sizeof(ts));
  snprintf(line, sizeof(line), "%s,%s", ts, NAMES[(int)p]);
  logAppend("/labterm/presence.csv", line, "time,state");
  return true;
}

Presence presenceGet() { return state; }
time_t presenceSince() { return since; }
time_t presenceArrived() { return arrived; }

uint32_t presenceTodaySec() {
  uint32_t s = inSec;
  if (state == Presence::In) s += time(nullptr) - since;
  return s;
}
