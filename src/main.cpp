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

// シリアルから時刻合わせ: "time <unix秒>"
static char line[96];
static size_t lineLen = 0;
static bool lineOverflow = false;

static void handleCommand(const char* s) {
  if (strncmp(s, "time ", 5) == 0) {
    char* end;
    long long v = strtoll(s + 5, &end, 10);
    if (*end != '\0' || v < 1700000000LL || v > 4102444800LL) {
      Serial.println("ERR bad time");
      return;
    }
    struct timeval tv = {(time_t)v, 0};
    settimeofday(&tv, nullptr);
    if (M5.Rtc.isEnabled()) saveRtc();
    Serial.println("OK");
  } else if (strcmp(s, "shot") == 0) {
    // canvasの中身をそのまま送る (RGB565 big endian)
    int w = canvas.width(), h = canvas.height();
    Serial.printf("SHOT %d %d\n", w, h);
    Serial.write((const uint8_t*)canvas.getBuffer(), w * h * 2);
    Serial.flush();
  } else {
    Serial.println("ERR unknown");
  }
}

static void pollSerial() {
  while (Serial.available()) {
    int c = Serial.read();
    if (c == '\r') continue;
    if (c == '\n') {
      line[lineLen] = '\0';
      if (!lineOverflow && lineLen > 0) handleCommand(line);
      lineLen = 0;
      lineOverflow = false;
    } else if (lineLen < sizeof(line) - 1) {
      line[lineLen++] = (char)c;
    } else {
      lineOverflow = true;
    }
  }
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
  auto mc = M5.config();
  mc.serial_baudrate = 115200;
  M5.begin(mc);
  loadConfig();

  // setSystemTimeFromRtc()の中でTZが壊れるので先に呼ぶ
  if (M5.Rtc.isEnabled()) {
    M5.Rtc.setSystemTimeFromRtc();
  }
  setenv("TZ", cfg.tz, 1);
  tzset();
  startNtp();

  canvas.setColorDepth(16);
  canvas.createSprite(M5.Display.width(), M5.Display.height());
}

void loop() {
  M5.update();
  pollSerial();

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
