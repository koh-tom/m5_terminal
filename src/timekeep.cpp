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
