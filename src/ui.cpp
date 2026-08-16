#include "ui.h"

#include <M5Unified.h>

#include "config.h"
#include "focus.h"
#include "presence.h"
#include "schedule.h"
#include "timekeep.h"

static constexpr uint16_t rgb(uint8_t r, uint8_t g, uint8_t b) {
  return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}

static constexpr uint16_t C_BG = rgb(16, 18, 24);
static constexpr uint16_t C_PANEL = rgb(40, 44, 56);
static constexpr uint16_t C_TEXT = rgb(235, 235, 235);
static constexpr uint16_t C_DIM = rgb(130, 136, 150);
static constexpr uint16_t C_WORK = rgb(235, 80, 70);
static constexpr uint16_t C_BREAK = rgb(70, 190, 120);
static constexpr uint16_t C_ACCENT = rgb(90, 160, 255);
static constexpr uint16_t C_WARN = rgb(245, 170, 50);
static constexpr uint16_t C_BUSY = rgb(130, 32, 32);

static constexpr int W = 320;
static constexpr int FOOTER_Y = UI_FOOTER_Y;

static M5Canvas canvas(&M5.Display);
static Screen screen = Screen::Clock;
static bool dirty = true;
static time_t lastNow = 0;
static uint32_t lastRemainSec = 0;

static const char* WDAY[] = {"日", "月", "火", "水", "木", "金", "土"};
static const char* TABS[] = {"時計", "集中", "在室", "記録"};

static char toast[64];
static uint32_t toastUntil = 0;
static uint32_t wakeAt = 0;
static int brightness = -1;

static bool isNight(const struct tm& t) {
  int m = t.tm_hour * 60 + t.tm_min;
  if (cfg.nightFrom == cfg.nightTo) return false;
  if (cfg.nightFrom < cfg.nightTo) return m >= cfg.nightFrom && m < cfg.nightTo;
  return m >= cfg.nightFrom || m < cfg.nightTo;  // 日付またぎ
}

static void updateBrightness(time_t now) {
  struct tm t;
  localtime_r(&now, &t);
  bool awake = wakeAt && millis() - wakeAt < 30000;
  int b = (isNight(t) && !awake) ? cfg.brightNight : cfg.brightDay;
  if (b != brightness) {
    brightness = b;
    M5.Display.setBrightness(b);
  }
}

static void drawFit(const char* str, int x, int y, int maxW);

static void drawHeader(const struct tm& t) {
  canvas.setFont(&fonts::lgfxJapanGothicP_16);

  // MTG前はヘッダを通知に使う
  int day = 0;
  const Item* mtg = reminderActive(&day);
  if (mtg) {
    char buf[96];
    snprintf(buf, sizeof(buf), "まもなく %d:%02d %s", mtg->start / 60, mtg->start % 60, mtg->title);
    canvas.fillRect(0, 0, W, UI_HEADER_H, C_WARN);
    canvas.setTextColor(C_BG);
    canvas.setTextDatum(middle_left);
    drawFit(buf, 8, UI_HEADER_H / 2, W - 16);
    return;
  }

  // 研究中は赤くして周りに分かるようにする
  bool busy = focusPhase() == Phase::Work && focusState() == RunState::Running;
  canvas.fillRect(0, 0, W, UI_HEADER_H, busy ? C_BUSY : C_PANEL);
  canvas.setTextDatum(middle_center);
  for (int i = 0; i < (int)Screen::Count; i++) {
    int x = i * UI_TAB_W;
    bool cur = i == (int)screen;
    canvas.setTextColor(cur ? C_TEXT : C_DIM);
    canvas.drawString(TABS[i], x + UI_TAB_W / 2, UI_HEADER_H / 2);
    if (cur) canvas.fillRect(x + 10, UI_HEADER_H - 3, UI_TAB_W - 20, 3, busy ? C_TEXT : C_ACCENT);
  }

  if (busy) {
    uint32_t sec = (focusRemainMs() + 999) / 1000;
    char buf[32];
    snprintf(buf, sizeof(buf), "研究中 %02lu:%02lu", (unsigned long)(sec / 60), (unsigned long)(sec % 60));
    canvas.setTextDatum(middle_right);
    canvas.setTextColor(C_TEXT);
    canvas.drawString(buf, W - 6, UI_HEADER_H / 2);
  } else if (screen != Screen::Clock) {
    char buf[8];
    snprintf(buf, sizeof(buf), "%02d:%02d", t.tm_hour, t.tm_min);
    canvas.setTextDatum(middle_right);
    canvas.setTextColor(C_TEXT);
    canvas.drawString(buf, W - 6, UI_HEADER_H / 2);
  }
}

static void drawFooter(const char* a, const char* b, const char* c) {
  const char* labels[] = {a, b, c};
  canvas.setFont(&fonts::lgfxJapanGothicP_16);
  canvas.setTextDatum(middle_center);
  for (int i = 0; i < 3; i++) {
    int x = i * W / 3;
    canvas.fillRoundRect(x + 6, FOOTER_Y + 6, W / 3 - 12, 30, 6, C_PANEL);
    canvas.setTextColor(C_TEXT);
    canvas.drawString(labels[i], x + W / 6, FOOTER_Y + 22);
  }
}

// 幅に収まらなければ後ろを削って…を付ける
static void drawFit(const char* str, int x, int y, int maxW) {
  if (canvas.textWidth(str) <= maxW) {
    canvas.drawString(str, x, y);
    return;
  }
  char buf[64];
  strlcpy(buf, str, sizeof(buf));
  size_t len = strlen(buf);
  while (len > 0) {
    do { len--; } while (len > 0 && (buf[len] & 0xC0) == 0x80);
    strcpy(buf + len, "…");
    if (canvas.textWidth(buf) <= maxW) break;
  }
  canvas.drawString(buf, x, y);
}

// 一番近い締切
static const Deadline* nextDeadline(int today, int* days) {
  const Deadline* best = nullptr;
  for (int i = 0; i < cfg.deadlineCount; i++) {
    int d = ymdDiff(today, cfg.deadlines[i].ymd);
    if (d < 0) continue;
    if (!best || d < *days) {
      best = &cfg.deadlines[i];
      *days = d;
    }
  }
  return best;
}

static void drawDeadline(int today) {
  int days = 0;
  const Deadline* dl = today ? nextDeadline(today, &days) : nullptr;
  if (!dl) return;

  const int x = 206, y = 32, w = 106, h = 84;
  uint16_t color = days <= 3 ? C_WORK : days <= 7 ? C_WARN : C_ACCENT;
  canvas.fillRoundRect(x, y, w, h, 8, C_PANEL);
  canvas.fillRect(x, y + 8, 4, h - 16, color);

  char buf[16];
  if (days == 0) {
    strcpy(buf, "D-DAY");
  } else {
    snprintf(buf, sizeof(buf), "D-%d", days);
  }
  canvas.setTextDatum(middle_center);
  canvas.setTextColor(color);
  canvas.setFont(days == 0 || days >= 100 ? &fonts::DejaVu24 : &fonts::DejaVu40);
  canvas.drawString(buf, x + w / 2 + 2, y + 32);

  canvas.setFont(&fonts::lgfxJapanGothicP_16);
  canvas.setTextColor(C_TEXT);
  drawFit(dl->title, x + w / 2 + 2, y + 66, w - 12);
}

static void fmtHm(char* buf, size_t n, int minutes) {
  snprintf(buf, n, "%d:%02d", minutes / 60, minutes % 60);
}

static void drawSchedule(time_t now, int today) {
  const int top = 124, rowH = 24, rows = 3;
  canvas.drawFastHLine(10, top - 4, W - 20, C_PANEL);
  canvas.setFont(&fonts::lgfxJapanGothicP_16);
  if (!today) return;

  const Item* list[MAX_ITEMS];
  int n = scheduleOfDay(today, list, MAX_ITEMS);

  int mtgDay = 0;
  const Item* mtg = nextMeeting(now, &mtgDay);

  // 終わった予定は飛ばして、なるべく先のを見せる
  int first = 0;
  while (first < n && itemEnd(list[first], today) <= now) first++;
  if (n - first < rows) first = max(0, n - rows);

  int y = top + rowH / 2;
  if (n == 0) {
    canvas.setTextDatum(middle_left);
    canvas.setTextColor(C_DIM);
    canvas.drawString("今日の予定なし", 14, y);
    y += rowH;
  }
  for (int i = first; i < n && i < first + rows; i++) {
    const Item* it = list[i];
    time_t st = itemStart(it, today), en = itemEnd(it, today);
    bool done = en <= now;
    bool active = st <= now && now < en;
    bool isNext = it == mtg && mtgDay == today;

    char hm[8];
    fmtHm(hm, sizeof(hm), it->start);
    canvas.setTextDatum(middle_left);
    canvas.setTextColor(done ? C_DIM : isNext ? C_ACCENT : C_TEXT);
    canvas.drawString(hm, 14, y);

    char right[24] = "";
    uint16_t rc = C_DIM;
    if (active) {
      strcpy(right, "進行中");
      rc = C_BREAK;
    } else if (isNext) {
      int mins = (int)((st - now + 59) / 60);
      if (mins >= 60) {
        snprintf(right, sizeof(right), "あと%d時間%d分", mins / 60, mins % 60);
      } else {
        snprintf(right, sizeof(right), "あと%d分", mins);
      }
      rc = C_ACCENT;
    }
    int rw = right[0] ? canvas.textWidth(right) + 8 : 0;
    drawFit(it->title, 64, y, W - 64 - 12 - rw);
    if (right[0]) {
      canvas.setTextDatum(middle_right);
      canvas.setTextColor(rc);
      canvas.drawString(right, W - 12, y);
    }
    y += rowH;
  }

  // 次のMTGが今日じゃなければ最後に出す
  if (mtg && mtgDay != today && y < UI_FOOTER_Y - 4) {
    char buf[96], hm[8];
    fmtHm(hm, sizeof(hm), mtg->start);
    int d = ymdDiff(today, mtgDay);
    if (d == 1) {
      snprintf(buf, sizeof(buf), "次: 明日 %s %s", hm, mtg->title);
    } else {
      snprintf(buf, sizeof(buf), "次: %d/%d(%s) %s %s", mtgDay / 100 % 100, mtgDay % 100,
               WDAY[ymdWeekday(mtgDay)], hm, mtg->title);
    }
    canvas.setTextDatum(middle_left);
    canvas.setTextColor(C_ACCENT);
    drawFit(buf, 14, y, W - 26);
  }
}

static void drawClock(time_t now, const struct tm& t) {
  char buf[16];
  snprintf(buf, sizeof(buf), "%02d:%02d", t.tm_hour, t.tm_min);
  canvas.setTextDatum(middle_left);
  canvas.setTextColor(C_TEXT);
  canvas.setFont(&fonts::DejaVu56);
  canvas.drawString(buf, 10, 64);

  char date[40];
  snprintf(date, sizeof(date), "%d月%d日 (%s)", t.tm_mon + 1, t.tm_mday, WDAY[t.tm_wday]);
  canvas.setFont(&fonts::lgfxJapanGothicP_20);
  canvas.drawString(date, 14, 106);

  int today = todayYmd();
  drawDeadline(today);
  drawSchedule(now, today);
}

static void drawFocus() {
  bool work = focusPhase() == Phase::Work;
  uint16_t color = work ? C_WORK : C_BREAK;

  canvas.setTextDatum(middle_center);
  canvas.setFont(&fonts::lgfxJapanGothicP_24);
  canvas.setTextColor(color);
  const char* label = work ? "研究" : "休憩";
  if (focusState() == RunState::Paused) {
    label = work ? "研究 (一時停止中)" : "休憩 (一時停止中)";
  }
  canvas.drawString(label, W / 2, 46);

  uint32_t sec = (focusRemainMs() + 999) / 1000;
  char buf[16];
  snprintf(buf, sizeof(buf), "%02lu:%02lu", (unsigned long)(sec / 60), (unsigned long)(sec % 60));
  canvas.setFont(&fonts::DejaVu72);
  canvas.setTextColor(focusState() == RunState::Running ? C_TEXT : C_DIM);
  canvas.drawString(buf, W / 2, 104);

  // 進捗バー
  int bw = 240, bx = (W - bw) / 2, by = 152;
  uint32_t len = focusLengthMs();
  int done = len ? (int)((uint64_t)(len - focusRemainMs()) * bw / len) : 0;
  canvas.fillRoundRect(bx, by, bw, 8, 4, C_PANEL);
  if (done > 0) canvas.fillRoundRect(bx, by, done, 8, 4, color);

  uint32_t total = focusTodaySec() / 60;
  char today[48];
  if (total >= 60) {
    snprintf(today, sizeof(today), "今日の研究 %lu時間%02lu分 (%d回)",
             (unsigned long)(total / 60), (unsigned long)(total % 60), focusTodayCount());
  } else {
    snprintf(today, sizeof(today), "今日の研究 %lu分 (%d回)", (unsigned long)total, focusTodayCount());
  }
  canvas.setFont(&fonts::lgfxJapanGothicP_16);
  canvas.setTextColor(C_DIM);
  canvas.drawString(today, W / 2, 180);
}

static void fmtDuration(char* buf, size_t n, uint32_t sec) {
  uint32_t m = sec / 60;
  if (m >= 60) {
    snprintf(buf, n, "%lu時間%02lu分", (unsigned long)(m / 60), (unsigned long)(m % 60));
  } else {
    snprintf(buf, n, "%lu分", (unsigned long)m);
  }
}

static void drawPresence() {
  Presence p = presenceGet();
  const char* label = p == Presence::In ? "在室" : p == Presence::Away ? "外出中" : "不在";
  uint16_t color = p == Presence::In ? C_BREAK : p == Presence::Away ? C_WARN : C_DIM;

  const int x = 20, y = 36, w = W - 40, h = 92;
  canvas.fillRoundRect(x, y, w, h, 10, C_PANEL);
  canvas.fillRect(x, y + 10, 5, h - 20, color);

  canvas.setTextDatum(middle_center);
  canvas.setTextColor(color);
  canvas.setFont(&fonts::lgfxJapanGothicP_40);
  canvas.drawString(label, W / 2, y + 36);

  char buf[64];
  canvas.setFont(&fonts::lgfxJapanGothicP_16);
  canvas.setTextColor(C_TEXT);
  if (p != Presence::Out && presenceSince()) {
    struct tm t;
    time_t since = presenceSince();
    localtime_r(&since, &t);
    snprintf(buf, sizeof(buf), "%d:%02d から", t.tm_hour, t.tm_min);
    canvas.drawString(buf, W / 2, y + 72);
  }

  canvas.setTextColor(C_DIM);
  if (presenceArrived()) {
    struct tm t;
    time_t arr = presenceArrived();
    localtime_r(&arr, &t);
    snprintf(buf, sizeof(buf), "今日の入室 %d:%02d", t.tm_hour, t.tm_min);
    canvas.drawString(buf, W / 2, 152);
  }
  char dur[24];
  fmtDuration(dur, sizeof(dur), presenceTodaySec());
  snprintf(buf, sizeof(buf), "在室時間 %s", dur);
  canvas.drawString(buf, W / 2, 176);
}

static void drawToast() {
  if (!toast[0]) return;
  if ((int32_t)(millis() - toastUntil) >= 0) {
    toast[0] = '\0';
    return;
  }
  canvas.setFont(&fonts::lgfxJapanGothicP_20);
  int w = canvas.textWidth(toast) + 32, h = 44;
  int x = (W - w) / 2, y = 80;
  canvas.fillRoundRect(x, y, w, h, 10, C_TEXT);
  canvas.setTextColor(C_BG);
  canvas.setTextDatum(middle_center);
  canvas.drawString(toast, W / 2, y + h / 2);
}

static void drawStats() {
  uint32_t week[7];
  focusWeek(week);

  uint32_t total = 0, maxSec = 3600;
  for (uint32_t v : week) {
    total += v;
    if (v > maxSec) maxSec = v;
  }

  char buf[48];
  canvas.setFont(&fonts::lgfxJapanGothicP_16);
  canvas.setTextColor(C_TEXT);
  canvas.setTextDatum(middle_left);
  canvas.drawString("この7日間", 12, 40);
  snprintf(buf, sizeof(buf), "合計 %.1fh / 平均 %.1fh", total / 3600.0f, total / 3600.0f / 7);
  canvas.setTextDatum(middle_right);
  canvas.drawString(buf, W - 12, 40);

  const int top = 66, bottom = 170, bw = 28, gap = 14;
  int x0 = (W - (bw * 7 + gap * 6)) / 2;
  int today = todayYmd();
  canvas.setTextDatum(bottom_center);
  for (int i = 0; i < 7; i++) {
    int x = x0 + i * (bw + gap);
    int h = (int)((uint64_t)week[i] * (bottom - top) / maxSec);
    canvas.fillRect(x, bottom - h, bw, h, i == 6 ? C_ACCENT : C_WORK);
    canvas.drawFastHLine(x - 4, bottom, bw + 8, C_PANEL);

    if (week[i] > 0) {
      snprintf(buf, sizeof(buf), "%.1f", week[i] / 3600.0f);
      canvas.setTextColor(C_DIM);
      canvas.drawString(buf, x + bw / 2, bottom - h - 2);
    }
    if (today) {
      canvas.setTextColor(i == 6 ? C_TEXT : C_DIM);
      canvas.drawString(WDAY[ymdWeekday(ymdAddDays(today, i - 6))], x + bw / 2, bottom + 22);
    }
  }
}

static void draw(time_t now) {
  struct tm t;
  localtime_r(&now, &t);

  canvas.fillScreen(C_BG);
  drawHeader(t);
  switch (screen) {
    case Screen::Clock: drawClock(now, t); break;
    case Screen::Focus: drawFocus(); break;
    case Screen::Presence: drawPresence(); break;
    case Screen::Stats: drawStats(); break;
    default: break;
  }
  if (screen == Screen::Presence) {
    drawFooter("入室", "外出", "退室");
  } else {
    drawFooter("開始", "停止", "リセット");
  }
  drawToast();
  canvas.pushSprite(0, 0);
}

void uiBegin() {
  canvas.setColorDepth(16);
  canvas.createSprite(M5.Display.width(), M5.Display.height());
}

void uiLoop() {
  time_t now = time(nullptr);
  uint32_t remainSec = (focusRemainMs() + 999) / 1000;
  if (toast[0] && (int32_t)(millis() - toastUntil) >= 0) dirty = true;
  if (dirty || now != lastNow || remainSec != lastRemainSec) {
    dirty = false;
    lastNow = now;
    lastRemainSec = remainSec;
    updateBrightness(now);
    draw(now);
  }
}

void uiDirty() { dirty = true; }

void uiWake() {
  wakeAt = millis();
  dirty = true;
}

void uiToast(const char* msg) {
  strlcpy(toast, msg, sizeof(toast));
  toastUntil = millis() + 1500;
  dirty = true;
}

Screen uiScreen() { return screen; }

void uiMoveScreen(int dir) {
  int n = (int)Screen::Count;
  screen = (Screen)(((int)screen + dir + n) % n);
  dirty = true;
}

void uiNextScreen() { uiMoveScreen(1); }

void uiSetScreen(Screen s) {
  if (s >= Screen::Count) return;
  screen = s;
  dirty = true;
}

// canvasの中身をそのまま送る (RGB565 big endian)
void uiShot() {
  int w = canvas.width(), h = canvas.height();
  Serial.printf("SHOT %d %d\n", w, h);

  // 一気に書くと取りこぼすので少しずつ
  const uint8_t* p = (const uint8_t*)canvas.getBuffer();
  size_t left = w * h * 2;
  uint32_t t0 = millis();
  while (left > 0 && millis() - t0 < 5000) {
    size_t n = Serial.write(p, min(left, (size_t)512));
    p += n;
    left -= n;
    if (n == 0) delay(1);
  }
  Serial.flush();
}
