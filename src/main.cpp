#include <M5Unified.h>
#include <WiFi.h>
#include <esp_sntp.h>
#include <time.h>

#include "config.h"

static M5Canvas canvas(&M5.Display);
static time_t lastDrawn = 0;
static volatile bool ntpSynced = false;

static void onNtpSync(struct timeval*) { ntpSynced = true; }

static void startNtp() {
  if (cfg.wifiCount == 0) return;
  WiFi.mode(WIFI_STA);
  WiFi.begin(cfg.wifi[0].ssid, cfg.wifi[0].pass);
  sntp_set_time_sync_notification_cb(onNtpSync);
  configTzTime(cfg.tz, cfg.ntp);
}

static void saveRtc() {
  time_t now = time(nullptr);
  struct tm t;
  gmtime_r(&now, &t);
  M5.Rtc.setDateTime(&t);
}

static const char* WDAY[] = {"日", "月", "火", "水", "木", "金", "土"};

static void draw(time_t now) {
  struct tm t;
  localtime_r(&now, &t);

  canvas.fillScreen(TFT_BLACK);
  canvas.setTextColor(TFT_WHITE);
  canvas.setTextDatum(middle_center);

  char buf[16];
  snprintf(buf, sizeof(buf), "%02d:%02d", t.tm_hour, t.tm_min);
  canvas.setFont(&fonts::DejaVu72);
  canvas.drawString(buf, 160, 100);

  char date[40];
  snprintf(date, sizeof(date), "%d月%d日 (%s)", t.tm_mon + 1, t.tm_mday, WDAY[t.tm_wday]);
  canvas.setFont(&fonts::lgfxJapanGothicP_24);
  canvas.drawString(date, 160, 160);

  canvas.pushSprite(0, 0);
}

void setup() {
  M5.begin();
  loadConfig();

  setenv("TZ", cfg.tz, 1);
  tzset();
  if (M5.Rtc.isEnabled()) {
    M5.Rtc.setSystemTimeFromRtc();
  }
  startNtp();

  canvas.setColorDepth(16);
  canvas.createSprite(M5.Display.width(), M5.Display.height());
}

void loop() {
  M5.update();

  if (ntpSynced) {
    ntpSynced = false;
    if (M5.Rtc.isEnabled()) saveRtc();
    Serial.println("[ntp] synced");
  }

  time_t now = time(nullptr);
  if (now != lastDrawn) {
    lastDrawn = now;
    draw(now);
  }
  delay(10);
}
