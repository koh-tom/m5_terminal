#include "ui.h"

#include <M5Unified.h>
#include <Preferences.h>

#include "config.h"
#include "focus.h"
#include "gfx.h"
#include "schedule.h"
#include "screens.h"
#include "tilt.h"
#include "timekeep.h"

M5Canvas canvas(&M5.Display);

static Screen screen = Screen::Clock;
static bool dirty = true;
static time_t lastNow = 0;
static uint32_t lastRemainSec = 0;
static uint32_t lastFrame = 0;

static Preferences prefs;
static bool analog = false;
static FocusStyle focusStyle = FocusStyle::Normal;

static const char* STYLE_NAMES[] = {"ふつう", "リング", "タイマー", "水", "マス", "砂時計", "エヴァ"};

static const char* TABS[] = {"時計", "予定", "集中", "在室", "記録"};

// QRカード
static bool card = false;
static bool cardForced = false;
static int cardIndex = 0;
static uint32_t cardAt = 0;
static uint8_t baseRotation = 1;

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

static void updateBrightness(const struct tm& t) {
  bool awake = wakeAt && millis() - wakeAt < 30000;
  int b = (isNight(t) && !awake) ? cfg.brightNight : cfg.brightDay;
  if (b != brightness) {
    brightness = b;
    M5.Display.setBrightness(b);
  }
}

static bool working() {
  return focusPhase() == Phase::Work && focusState() == RunState::Running;
}

static void drawHeader() {
  // MTG前はヘッダを通知に使う
  int day = 0;
  const Item* mtg = reminderActive(&day);
  if (mtg) {
    char buf[96];
    snprintf(buf, sizeof(buf), "まもなく %d:%02d  %s", mtg->start / 60, mtg->start % 60, mtg->title);
    canvas.fillRect(0, 0, UI_W, UI_HEADER_H, col::AMBER);
    textFit(buf, 10, UI_HEADER_H / 2, UI_W - 20, Font::Jp13, col::BG, middle_left);
    return;
  }

  bool busy = working();
  bool evaFocus = focusStyle == FocusStyle::Eva && screen == Screen::Focus;
  canvas.fillRect(0, 0, UI_W, UI_HEADER_H, evaFocus ? TFT_BLACK : col::BG);
  if (busy) canvas.fillRect(0, 0, UI_W, 2, col::RED);

  for (int i = 0; i < (int)Screen::Count; i++) {
    int cx = i * UI_TAB_W + UI_TAB_W / 2;
    bool cur = i == (int)screen;
    const char* label = TABS[i];
    uint16_t color = cur ? col::TEXT : col::MUTED;

    // 研究中は集中タブに残り時間を出す
    char rem[8];
    if (busy && i == (int)Screen::Focus) {
      uint32_t sec = (focusRemainMs() + 999) / 1000;
      snprintf(rem, sizeof(rem), "%02lu:%02lu", (unsigned long)(sec / 60), (unsigned long)(sec % 60));
      label = rem;
      color = col::RED;
    }
    text(label, cx, UI_HEADER_H / 2, Font::Jp13, color, middle_center);
    if (cur) pill(cx - 9, UI_HEADER_H - 4, 18, 3, busy && i == (int)Screen::Focus ? col::RED : col::ACCENT);
  }
}

static void drawToast() {
  if (!toast[0]) return;
  if ((int32_t)(millis() - toastUntil) >= 0) {
    toast[0] = '\0';
    return;
  }
  int w = textWidth(toast, Font::Jp16) + 40, h = 40;
  int x = (UI_W - w) / 2, y = 96;
  roundRect(x, y, w, h, 12, col::SURFACE2);
  text(toast, UI_W / 2, y + h / 2, Font::Jp16, col::TEXT, middle_center);
}

static void draw(time_t now) {
  if (card) {
    drawCard(cardIndex);
    canvas.pushSprite(0, 0);
    return;
  }

  Frame f;
  f.now = now;
  localtime_r(&now, &f.t);
  f.today = todayYmd();
  updateBrightness(f.t);

  canvas.fillScreen(col::BG);
  switch (screen) {
    case Screen::Clock: drawClock(f, analog); break;
    case Screen::Agenda: drawAgenda(f); break;
    case Screen::Focus:
      switch (focusStyle) {
        case FocusStyle::Ring: drawFocusRing(f); break;
        case FocusStyle::Dial: drawFocusDial(f); break;
        case FocusStyle::Water: drawFocusWater(f); break;
        case FocusStyle::Grid: drawFocusGrid(f); break;
        case FocusStyle::Hourglass: drawFocusHourglass(f); break;
        case FocusStyle::Eva: drawEva(f); break;
        default: drawFocus(f); break;
      }
      break;
    case Screen::Presence: drawPresence(f); break;
    case Screen::Stats: drawStats(f); break;
    default: break;
  }
  if (screen == Screen::Focus) {
    if (focusStyle == FocusStyle::Eva) {
      drawEvaFooter();
    } else {
      drawFocusFooter();
    }
  } else if (screen == Screen::Presence) {
    drawPresenceFooter();
  }
  drawHeader();
  drawToast();
  canvas.pushSprite(0, 0);
}

void uiBegin() {
  baseRotation = M5.Display.getRotation();
  prefs.begin("ui", false);
  analog = prefs.getBool("analog", cfg.clockAnalog);
  int fs = prefs.getUChar("fstyle", cfg.focusStyle);
  focusStyle = fs < (int)FocusStyle::Count ? (FocusStyle)fs : FocusStyle::Normal;

  // PSRAMが無い機種(Basic)はメモリが足りないので8bitにする
  bool psram = psramFound();
  canvas.setPsram(psram);
  canvas.setColorDepth(psram ? 16 : 8);
  if (!canvas.createSprite(M5.Display.width(), M5.Display.height())) {
    Serial.println("[ui] createSprite failed");
  }
}

// 逆さまにしたら画面を180度回してQRを出す
static void updateCard() {
  bool want = cfg.linkCount > 0 && (cardForced || tiltUpsideDown());
  if (want != card) {
    card = want;
    cardIndex = 0;
    cardAt = millis();
    M5.Display.setRotation(card ? (baseRotation + 2) % 4 : baseRotation);
    dirty = true;
  }
  // 8秒ごとに次のリンク
  if (card && cfg.linkCount > 1 && millis() - cardAt > 8000) uiCardNext();
}

void uiLoop() {
  updateCard();
  time_t now = time(nullptr);
  uint32_t remainSec = (focusRemainMs() + 999) / 1000;
  if (toast[0] && (int32_t)(millis() - toastUntil) >= 0) dirty = true;

  // エヴァは1/100秒、水は波を動かすので速めに描く
  if (screen == Screen::Focus && focusState() == RunState::Running) {
    uint32_t interval = focusStyle == FocusStyle::Eva ? 33 : focusStyle == FocusStyle::Water ? 66 : 0;
    if (interval && millis() - lastFrame >= interval) dirty = true;
  }

  if (dirty || now != lastNow || remainSec != lastRemainSec) {
    dirty = false;
    lastNow = now;
    lastRemainSec = remainSec;
    lastFrame = millis();
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
  int i = (int)s;
  if (i < 0 || i >= (int)Screen::Count) return;
  screen = s;
  dirty = true;
}

bool uiHasFooter() {
  return screen == Screen::Focus || screen == Screen::Presence;
}

void uiToggleStyle() {
  if (screen == Screen::Clock) {
    analog = !analog;
    prefs.putBool("analog", analog);
  } else if (screen == Screen::Focus) {
    focusStyle = (FocusStyle)(((int)focusStyle + 1) % (int)FocusStyle::Count);
    prefs.putUChar("fstyle", (uint8_t)focusStyle);
    uiToast(STYLE_NAMES[(int)focusStyle]);
  }
  dirty = true;
}

// canvasの中身を送る (RGB565 big endian)
void uiShot() {
  int w = canvas.width(), h = canvas.height();
  Serial.printf("SHOT %d %d\n", w, h);

  // 1行ずつRGB565に直して送る (一気に書くと取りこぼす)
  uint8_t row[320 * 2];
  for (int y = 0; y < h; y++) {
    for (int x = 0; x < w && x < 320; x++) {
      uint16_t c = canvas.readPixel(x, y);
      row[x * 2] = c >> 8;
      row[x * 2 + 1] = c & 0xFF;
    }
    const uint8_t* p = row;
    size_t left = w * 2;
    uint32_t t0 = millis();
    while (left > 0 && millis() - t0 < 1000) {
      size_t n = Serial.write(p, left);
      p += n;
      left -= n;
      if (n == 0) delay(1);
    }
  }
  Serial.flush();
}

bool uiCardActive() { return card; }

void uiCardNext() {
  if (cfg.linkCount == 0) return;
  cardIndex = (cardIndex + 1) % cfg.linkCount;
  cardAt = millis();
  dirty = true;
}

void uiCardForce(bool on) {
  cardForced = on;
}
