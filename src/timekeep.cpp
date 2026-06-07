#include "timekeep.h"

#include <M5Unified.h>
#include <WiFi.h>
#include <esp_sntp.h>

#include "config.h"

static volatile bool ntpSynced = false;

static void onNtpSync(struct timeval*) { ntpSynced = true; }

static void saveRtc() {
  if (!M5.Rtc.isEnabled()) return;
  time_t now = time(nullptr);
  struct tm t;
  gmtime_r(&now, &t);
  M5.Rtc.setDateTime(&t);
}

void timeBegin() {
  // setSystemTimeFromRtc()の中でTZが壊れるので先に呼ぶ
  if (M5.Rtc.isEnabled()) {
    M5.Rtc.setSystemTimeFromRtc();
  }
  setenv("TZ", cfg.tz, 1);
  tzset();

  if (cfg.wifiCount == 0) return;
  WiFi.mode(WIFI_STA);
  WiFi.begin(cfg.wifi[0].ssid, cfg.wifi[0].pass);
  sntp_set_time_sync_notification_cb(onNtpSync);
  configTzTime(cfg.tz, cfg.ntp);
}

void timeLoop() {
  if (ntpSynced) {
    ntpSynced = false;
    saveRtc();
    Serial.println("[ntp] synced");
  }
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
