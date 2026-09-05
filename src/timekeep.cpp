#include "timekeep.h"

#include <M5Unified.h>
#include <WiFi.h>
#include <esp_sntp.h>

#include "config.h"

// wifiは時刻合わせの時だけつなぐ
enum class Net { Off, Connecting, Syncing };

static Net net = Net::Off;
static int apIndex = 0;
static uint32_t stepAt = 0;
static uint32_t nextSyncAt = 0;
static time_t lastSync = 0;
static volatile bool ntpSynced = false;

static void onNtpSync(struct timeval*) { ntpSynced = true; }

static void saveRtc() {
  if (!M5.Rtc.isEnabled()) return;
  time_t now = time(nullptr);
  struct tm t;
  gmtime_r(&now, &t);
  M5.Rtc.setDateTime(&t);
}

static void connect() {
  Serial.printf("[wifi] connecting to %s\n", cfg.wifi[apIndex].ssid);
  WiFi.mode(WIFI_STA);
  WiFi.setHostname("labterm");
  WiFi.begin(cfg.wifi[apIndex].ssid, cfg.wifi[apIndex].pass);
  net = Net::Connecting;
  stepAt = millis();
}

static void stop(bool ok) {
  sntp_stop();
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
  net = Net::Off;
  // 成功したら6時間後、失敗したら10分後にまた試す
  nextSyncAt = millis() + (ok ? 6 * 3600 : 600) * 1000UL;
  Serial.printf("[wifi] off (%s)\n", ok ? "synced" : "failed");
}

void timeBegin() {
  // setSystemTimeFromRtc()の中でTZが壊れるので先に呼ぶ
  if (M5.Rtc.isEnabled()) {
    M5.Rtc.setSystemTimeFromRtc();
  }
  setenv("TZ", cfg.tz, 1);
  tzset();
  sntp_set_time_sync_notification_cb(onNtpSync);
}

void timeLoop() {
  switch (net) {
    case Net::Off:
      if (cfg.wifiCount > 0 && (int32_t)(millis() - nextSyncAt) >= 0) {
        apIndex = 0;
        connect();
      }
      break;

    case Net::Connecting:
      if (WiFi.status() == WL_CONNECTED) {
        configTzTime(cfg.tz, cfg.ntp);
        net = Net::Syncing;
        stepAt = millis();
      } else if (millis() - stepAt > 15000) {
        WiFi.disconnect();
        if (++apIndex < cfg.wifiCount) {
          connect();
        } else {
          stop(false);
        }
      }
      break;

    case Net::Syncing:
      if (ntpSynced) {
        ntpSynced = false;
        saveRtc();
        lastSync = time(nullptr);
        stop(true);
      } else if (millis() - stepAt > 20000) {
        stop(false);
      }
      break;
  }
}

time_t lastNtpSync() { return lastSync; }

void timeResync() {
  if (net == Net::Off) nextSyncAt = millis();
}

void timeSet(time_t t) {
  struct timeval tv = {t, 0};
  settimeofday(&tv, nullptr);
  saveRtc();
}

bool timeValid() {
  return time(nullptr) > 1735689600;  // 2025-01-01
}

int todayYmd() {
  if (!timeValid()) return 0;
  time_t now = time(nullptr);
  struct tm t;
  localtime_r(&now, &t);
  return (t.tm_year + 1900) * 10000 + (t.tm_mon + 1) * 100 + t.tm_mday;
}

static struct tm ymdToTm(int ymd) {
  struct tm t = {};
  t.tm_year = ymd / 10000 - 1900;
  t.tm_mon = ymd / 100 % 100 - 1;
  t.tm_mday = ymd % 100;
  t.tm_hour = 12;
  t.tm_isdst = -1;
  return t;
}

int ymdAddDays(int ymd, int days) {
  struct tm t = ymdToTm(ymd);
  t.tm_mday += days;
  mktime(&t);
  return (t.tm_year + 1900) * 10000 + (t.tm_mon + 1) * 100 + t.tm_mday;
}

int ymdWeekday(int ymd) {
  struct tm t = ymdToTm(ymd);
  mktime(&t);
  return t.tm_wday;
}

int ymdDiff(int from, int to) {
  struct tm a = ymdToTm(from), b = ymdToTm(to);
  double sec = difftime(mktime(&b), mktime(&a));
  return (int)lround(sec / 86400.0);
}

void fmtTime(time_t t, char* buf, size_t n) {
  struct tm tm;
  localtime_r(&t, &tm);
  strftime(buf, n, "%Y-%m-%d %H:%M:%S", &tm);
}
