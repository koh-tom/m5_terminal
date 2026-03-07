#include <M5Unified.h>
#include <time.h>

static M5Canvas canvas(&M5.Display);
static time_t lastDrawn = 0;

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

  snprintf(buf, sizeof(buf), "%04d/%02d/%02d", t.tm_year + 1900, t.tm_mon + 1, t.tm_mday);
  canvas.setFont(&fonts::DejaVu24);
  canvas.drawString(buf, 160, 160);

  canvas.pushSprite(0, 0);
}

void setup() {
  M5.begin();

  setenv("TZ", "JST-9", 1);
  tzset();
  if (M5.Rtc.isEnabled()) {
    M5.Rtc.setSystemTimeFromRtc();
  }

  canvas.setColorDepth(16);
  canvas.createSprite(M5.Display.width(), M5.Display.height());
}

void loop() {
  M5.update();

  time_t now = time(nullptr);
  if (now != lastDrawn) {
    lastDrawn = now;
    draw(now);
  }
  delay(10);
}
