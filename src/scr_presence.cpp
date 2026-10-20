#include "gfx.h"
#include "presence.h"
#include "screens.h"
#include "ui.h"

static uint16_t stateColor(Presence p) {
  return p == Presence::In ? col::GREEN : p == Presence::Away ? col::AMBER : col::MUTED;
}

static void tile(int x, int y, int w, const char* label, const char* value) {
  roundRect(x, y, w, 48, 12, col::SURFACE);
  text(label, x + 14, y + 14, Font::Jp13, col::MUTED, middle_left);
  text(value, x + 14, y + 33, Font::Jp16, col::TEXT, middle_left);
}

void drawPresence(const Frame& f) {
  (void)f;
  Presence p = presenceGet();
  const char* label = p == Presence::In ? "在室" : p == Presence::Away ? "外出中" : "不在";
  uint16_t color = stateColor(p);

  roundRect(12, 36, UI_W - 24, 92, 14, col::SURFACE);
  text(label, UI_W / 2, 72, Font::Jp36, color, middle_center);

  char buf[48];
  if (p != Presence::Out && presenceSince()) {
    struct tm t;
    time_t since = presenceSince();
    localtime_r(&since, &t);
    snprintf(buf, sizeof(buf), "%d:%02d から", t.tm_hour, t.tm_min);
    text(buf, UI_W / 2, 110, Font::Jp13, col::SUB, middle_center);
  }

  const int tw = (UI_W - 24 - 10) / 2;
  if (presenceArrived()) {
    struct tm t;
    time_t arr = presenceArrived();
    localtime_r(&arr, &t);
    snprintf(buf, sizeof(buf), "%d:%02d", t.tm_hour, t.tm_min);
  } else {
    strcpy(buf, "-");
  }
  tile(12, 138, tw, "今日の入室", buf);
  fmtDuration(buf, sizeof(buf), presenceTodaySec());
  tile(12 + tw + 10, 138, tw, "在室時間", buf);
}

void drawPresenceFooter() {
  static const Presence ps[] = {Presence::In, Presence::Away, Presence::Out};
  static const char* labels[] = {"入室", "外出", "退室"};
  const int bw = 92, h = 32, y = UI_FOOTER_Y + 6;
  for (int i = 0; i < 3; i++) {
    int x = 10 + i * (bw + 12);
    bool cur = presenceGet() == ps[i];
    pill(x, y, bw, h, cur ? stateColor(ps[i]) : col::SURFACE);
    text(labels[i], x + bw / 2, y + h / 2, Font::Jp13, cur ? col::BG : col::TEXT, middle_center);
  }
}
