#include "nfc.h"

#include <M5UnitUnified.h>
#include <M5UnitUnifiedNFC.h>
#include <M5Unified.h>
#include <Preferences.h>
#include <esp_random.h>
#include <mbedtls/md.h>

#include <algorithm>
#include <string>
#include <vector>

#include "beep.h"
#include "presence.h"
#include "timekeep.h"
#include "ui.h"

using m5::nfc::a::PICC;
using m5::nfc::a::Type;

namespace {
m5::unit::UnitUnified units;
m5::unit::UnitNFC unit{};
m5::nfc::NFCLayerA nfcA{unit};
m5::nfc::NFCLayerF nfcF{unit};
bool ready = false;

constexpr int MAX_CARDS = 8;
constexpr uint32_t UNLOCK_MS = 10000;
constexpr uint32_t POLL_MS = 400;
constexpr uint32_t GONE_MS = 1500;  // これだけ見えなかったら離したとみなす

Preferences prefs;
uint8_t key[32];  // カードIDのハッシュとカードのパスワードを作る鍵 (端末ごと)
uint8_t cards[MAX_CARDS][16];
int cardCount = 0;
uint32_t unlockedAt = 0;
bool unlocked = false;

// ---- カードに書く記録 (NTAG213/215/216 のユーザー領域) ----
// page 4-8: ヘッダ、page 9-: 4バイトずつのリング (上2bitが状態、下30bitが2026年からの秒)
constexpr uint8_t HEAD_PAGE = 4;
constexpr uint8_t LOG_PAGE = 9;
constexpr uint32_t EPOCH_BASE = 1767225600;  // 2026-01-01 00:00 UTC
constexpr uint32_t MAX_STAY = 16 * 3600;    // 退室し忘れはこれ以上数えない

struct __attribute__((packed)) CardHead {
  char magic[4];  // "LTR1"
  uint16_t ym;    // 今月 (年*12+月)
  uint32_t curSec;
  uint32_t prevSec;
  uint16_t days;     // 入室した日数 (通算)
  uint16_t lastDay;  // 最後に入室した日 ((年-2026)*366+通日)
  uint8_t head;      // 次に書くリングの位置
  uint8_t count;
};
static_assert(sizeof(CardHead) == 20, "head is 5 pages");

void hmac(const char* msg, uint8_t out[32]) {
  mbedtls_md_hmac(mbedtls_md_info_from_type(MBEDTLS_MD_SHA256), key, sizeof(key), (const uint8_t*)msg,
                  strlen(msg), out);
}

void cardHash(const std::string& id, uint8_t out[16]) {
  uint8_t h[32];
  hmac(("id:" + id).c_str(), h);
  memcpy(out, h, 16);
}

int findCard(const uint8_t h[16]) {
  for (int i = 0; i < cardCount; i++) {
    if (memcmp(cards[i], h, 16) == 0) return i;
  }
  return -1;
}

void saveCards() {
  prefs.putBytes("cards", cards, cardCount * 16);
}

bool isNtag21x(const PICC& p) {
  return p.type == Type::NTAG_213 || p.type == Type::NTAG_215 || p.type == Type::NTAG_216;
}

// CFG0 はユーザー領域と動的ロックの次のページ
uint8_t cfgPage(const PICC& p) { return p.lastUserBlock() + 2; }
uint16_t logCap(const PICC& p) { return std::min<int>(120, p.lastUserBlock() - LOG_PAGE + 1); }

void cardPassword(const PICC& p, uint8_t pwd[4], uint8_t pack[2]) {
  uint8_t h[32];
  hmac(("pwd:" + p.uidAsString()).c_str(), h);
  memcpy(pwd, h, 4);
  memcpy(pack, h + 4, 2);
}

bool pwdAuth(const PICC& p) {
  uint8_t pwd[4], pack[2];
  cardPassword(p, pwd, pack);
  uint8_t tx[5] = {0x1B, pwd[0], pwd[1], pwd[2], pwd[3]};
  uint8_t rx[4]{};
  uint16_t len = sizeof(rx);
  return nfcA.transceive(rx, len, tx, sizeof(tx), 20) && len >= 2 && rx[0] == pack[0] && rx[1] == pack[1];
}

// 守られていなければそのまま、守られていれば自分のパスワードで開ける
bool cardOpen(const PICC& p, bool& protectedByUs) {
  uint8_t cfg[4];
  if (nfcA.read4(cfg, cfgPage(p)) && cfg[3] > p.lastUserBlock()) {
    protectedByUs = false;
    return true;
  }
  // 読みも守っているとCFGも読めない。NAKの後は選び直す
  protectedByUs = true;
  nfcA.reactivate(p);
  return pwdAuth(p);
}

// 書けたのにACKを取りこぼすことがあるので、失敗したら読み直して確かめる
bool writePage(uint8_t page, const uint8_t* data, bool safety = true) {
  for (int i = 0; i < 3; i++) {
    if (nfcA.write4(page, data, 4, safety)) return true;
    uint8_t back[4];
    if (nfcA.read4(back, page) && memcmp(back, data, 4) == 0) return true;
  }
  Serial.printf("[nfc] write page %u failed\n", page);
  return false;
}

bool readHead(CardHead& h) {
  uint8_t buf[20];
  if (!nfcA.read16(buf, HEAD_PAGE) || !nfcA.read4(buf + 16, HEAD_PAGE + 4)) return false;
  memcpy(&h, buf, sizeof(h));
  return memcmp(h.magic, "LTR1", 4) == 0;
}

bool writeHead(const CardHead& h) {
  const uint8_t* b = (const uint8_t*)&h;
  for (int i = 0; i < 5; i++) {
    if (!writePage(HEAD_PAGE + i, b + i * 4)) return false;
  }
  return true;
}

uint16_t monthOf(time_t t) {
  struct tm tm;
  localtime_r(&t, &tm);
  return (tm.tm_year + 1900) * 12 + tm.tm_mon;
}

uint16_t dayOf(time_t t) {
  struct tm tm;
  localtime_r(&t, &tm);
  return (uint16_t)((tm.tm_year - 126) * 366 + tm.tm_yday);
}

uint32_t packEntry(Presence s, time_t t) { return ((uint32_t)s << 30) | ((uint32_t)(t - EPOCH_BASE) & 0x3FFFFFFF); }
Presence entryState(uint32_t e) { return (Presence)(e >> 30); }
time_t entryTime(uint32_t e) { return (time_t)(e & 0x3FFFFFFF) + EPOCH_BASE; }

bool readEntry(const PICC& p, const CardHead& h, int back, uint32_t& e) {
  uint16_t cap = logCap(p);
  if (back >= h.count) return false;
  int idx = ((int)h.head - 1 - back + cap * 2) % cap;
  return nfcA.read4((uint8_t*)&e, LOG_PAGE + idx);
}

// かざした結果をカードにも残す。今月の在室時間を返す (書けなければ -1)
long cardRecord(const PICC& p, Presence s, time_t now) {
  if (!isNtag21x(p)) return -1;
  bool prot;
  CardHead h;
  if (!cardOpen(p, prot) || !prot || !readHead(h)) return -1;

  uint16_t ym = monthOf(now);
  if (h.ym != ym) {
    h.prevSec = (h.ym + 1 == ym) ? h.curSec : 0;
    h.curSec = 0;
    h.ym = ym;
  }
  uint32_t last;
  if (readEntry(p, h, 0, last) && entryState(last) == Presence::In && now > entryTime(last)) {
    h.curSec += std::min<uint32_t>(now - entryTime(last), MAX_STAY);
  }
  if (s == Presence::In && h.lastDay != dayOf(now)) {
    h.days++;
    h.lastDay = dayOf(now);
  }

  uint32_t e = packEntry(s, now);
  uint16_t cap = logCap(p);
  if (!writePage(LOG_PAGE + h.head % cap, (uint8_t*)&e)) return -1;
  h.head = (h.head + 1) % cap;
  if (h.count < cap) h.count++;
  return writeHead(h) ? (long)h.curSec : -1;
}

// 記録用に初期化して、パスワードで読み書きを守る
bool cardFormat(const PICC& p, Print& out) {
  if (!isNtag21x(p)) {
    out.println("not NTAG213/215/216 (id only)");
    return false;
  }
  bool prot;
  if (!cardOpen(p, prot)) {
    out.println("protected by someone else (id only)");
    return false;
  }
  uint8_t lock[4];
  if (nfcA.read4(lock, 2) && (lock[2] || lock[3])) {
    out.println("pages are locked (id only)");
    return false;
  }

  CardHead h{};
  memcpy(h.magic, "LTR1", 4);
  h.ym = monthOf(time(nullptr));
  if (!writeHead(h)) {
    out.println("write failed");
    return false;
  }
  if (prot) return true;

  // PWD, PACK, CFG1 (PROT=1: 読みも守る, AUTHLIM=0), 最後に CFG0 の AUTH0
  uint8_t cfg = cfgPage(p);
  uint8_t pwd[4], pack[2], c0[4], c1[4];
  cardPassword(p, pwd, pack);
  uint8_t pk[4] = {pack[0], pack[1], 0, 0};
  if (!nfcA.read4(c0, cfg) || !nfcA.read4(c1, cfg + 1)) return false;
  c1[0] = (c1[0] | 0x80) & ~0x07;
  c0[3] = HEAD_PAGE;
  // PWD/PACKは読み返せないので、書いた後に認証できるか確かめてから守る
  nfcA.write4(cfg + 2, pwd, 4, false);
  nfcA.write4(cfg + 3, pk, 4, false);
  nfcA.reactivate(p);
  if (!pwdAuth(p)) {
    out.println("password check failed (not protected)");
    return false;
  }
  bool ok = writePage(cfg + 1, c1, false) && writePage(cfg, c0, false);
  out.println(ok ? "card formatted and protected" : "protect failed");
  return ok;
}

// ---- 読み取り ----

struct Seen {
  bool any;
  bool isA;
  PICC a;
  std::string id;
};

// カードを1枚探す。NFC-A は選択した状態で返す
Seen detect(uint32_t timeoutMs = 40) {
  Seen s{};
  units.update();
  PICC p{};
  // A と F は読み方 (モード) を切り替えてから探す
  unit.configureNFCMode(m5::nfc::NFC::A);
  if (nfcA.detect(p, timeoutMs)) {
    if (nfcA.identify(p) && nfcA.reactivate(p)) {
      s = {true, true, p, "A:" + p.uidAsString()};
    } else {
      nfcA.deactivate();
    }
    return s;
  }
  m5::nfc::f::PICC f{};
  unit.configureNFCMode(m5::nfc::NFC::F);
  if (nfcF.detect(f, timeoutMs)) s = {true, false, {}, "F:" + f.idmAsString()};
  return s;
}

void done(const Seen& s) {
  if (s.isA) nfcA.deactivate();
}

void onCard(const Seen& s) {
  uint8_t h[16];
  cardHash(s.id, h);
  uiWake();
  if (findCard(h) < 0) {
    uiToast("登録されていないカードです");
    beep(2, 800);
    return;
  }
  unlocked = true;
  unlockedAt = millis();
  uiSetScreen(Screen::Presence);

  if (!timeValid()) {
    uiToast("時刻が未設定です");
    return;
  }
  // 外出中にかざしたら戻ってきた扱い
  Presence to = presenceGet() == Presence::In ? Presence::Out : Presence::In;
  if (!presenceSet(to, PresenceSource::Card)) return;
  beep(1);

  long month = s.isA ? cardRecord(s.a, to, time(nullptr)) : -1;
  char msg[48];
  if (to == Presence::In) {
    strcpy(msg, "入室しました");
  } else if (month >= 0) {
    snprintf(msg, sizeof(msg), "おつかれさま 今月 %ld:%02ld", month / 3600, month / 60 % 60);
  } else {
    strcpy(msg, "おつかれさまでした");
  }
  uiToast(msg);
  uiDirty();
}

// 次にかざしたカードを待つ (USBのコマンド用)
bool waitCard(Seen& s, Print& out, uint32_t ms = 10000) {
  out.println("place a card...");
  uint32_t t0 = millis();
  while (millis() - t0 < ms) {
    // 一度読んで止めた (HALT) カードも起こすため、電波を一度切る
    unit.disableField();
    delay(10);
    unit.enableField();
    delay(5);
    s = detect(1000);
    if (s.any) return true;
    delay(100);
  }
  out.println("ERR no card");
  return false;
}

void cmdAdd(Print& out) {
  Seen s;
  if (!waitCard(s, out)) return;
  uint8_t h[16];
  cardHash(s.id, h);
  if (findCard(h) >= 0) {
    out.println("already registered");
  } else if (cardCount >= MAX_CARDS) {
    out.println("ERR full");
    done(s);
    return;
  } else {
    memcpy(cards[cardCount++], h, 16);
    saveCards();
    out.printf("registered #%d\n", cardCount - 1);
  }
  if (s.isA) {
    CardHead head;
    bool prot;
    if (isNtag21x(s.a) && cardOpen(s.a, prot) && prot && readHead(head)) {
      out.println("card log kept");
    } else {
      // 認証に失敗したカードは選び直してから書く
      nfcA.reactivate(s.a);
      cardFormat(s.a, out);
    }
  }
  done(s);
}

void cmdRead(Print& out) {
  Seen s;
  if (!waitCard(s, out)) return;
  uint8_t h[16];
  cardHash(s.id, h);
  out.printf("%s %s\n", s.isA ? s.a.typeAsString().c_str() : "FeliCa",
             findCard(h) >= 0 ? "registered" : "not registered");
  CardHead head;
  bool prot;
  if (!s.isA || !isNtag21x(s.a) || !cardOpen(s.a, prot) || !readHead(head)) {
    out.println("no card log");
    done(s);
    return;
  }
  out.println(prot ? "protected" : "not protected");
  out.printf("month %u/%u %lus, prev %lus, days %u\n", head.ym / 12, head.ym % 12 + 1,
             (unsigned long)head.curSec, (unsigned long)head.prevSec, head.days);
  for (int i = head.count - 1; i >= 0; i--) {
    uint32_t e;
    if (!readEntry(s.a, head, i, e)) break;
    char buf[24];
    fmtTime(entryTime(e), buf, sizeof(buf));
    out.printf("%s %s\n", buf, presenceName(entryState(e)));
  }
  done(s);
}

void cmdProbe(const char* arg, Print& out) {
  uint32_t t0 = millis();
  Seen s = detect(atoi(arg) > 0 ? atoi(arg) : 40);
  out.printf("%lums\n", (unsigned long)(millis() - t0));
  if (!s.any) {
    out.println("no card");
  } else if (s.isA) {
    out.printf("A %s uid=%s user=%u\n", s.a.typeAsString().c_str(), s.a.uidAsString().c_str(),
               (unsigned)s.a.userAreaSize());
  } else {
    out.println(s.id.c_str());
  }
  done(s);
}
}  // namespace

bool nfcBegin() {
  prefs.begin("nfc", false);
  if (prefs.getBytes("key", key, sizeof(key)) != sizeof(key)) {
    esp_fill_random(key, sizeof(key));
    prefs.putBytes("key", key, sizeof(key));
  }
  cardCount = prefs.getBytes("cards", cards, sizeof(cards)) / 16;

  M5.Ex_I2C.begin();
  if (!M5.Ex_I2C.scanID(0x50)) {
    Serial.println("[nfc] not found");
    return false;
  }
  // 起動直後は失敗することがあるので何回か試す
  if (!units.add(unit, M5.Ex_I2C)) return false;
  for (int i = 0; i < 5 && !ready; i++) {
    ready = units.begin();
    if (!ready) delay(200);
  }
  Serial.printf("[nfc] %s, %d cards\n", ready ? "ready" : "init failed", cardCount);
  return ready;
}

bool nfcReady() { return ready; }

bool nfcLocked() {
  if (!ready || cardCount == 0) return false;
  if (unlocked && millis() - unlockedAt < UNLOCK_MS) return false;
  unlocked = false;
  return true;
}

void nfcLoop() {
  if (!ready) return;
  static uint32_t lastPoll = 0, lastSeen = 0;
  static std::string lastId;
  if (millis() - lastPoll < POLL_MS) return;
  lastPoll = millis();

  Seen s = detect();
  if (!s.any) return;
  // 置いたままのカードは1回だけ
  // 起動した時に置いてあったカードも「置いたまま」扱い
  bool again = (s.id == lastId && millis() - lastSeen < GONE_MS) || millis() < 5000;
  lastSeen = millis();
  lastId = s.id;
  if (!again) onCard(s);
  done(s);
}

void nfcCommand(const char* args, Print& out) {
  if (!ready) {
    out.println("ERR nfc not ready");
    return;
  }
  if (*args == '\0' || strncmp(args, "probe", 5) == 0) {
    cmdProbe(*args ? args + 5 : "", out);
  } else if (strcmp(args, "add") == 0) {
    cmdAdd(out);
  } else if (strcmp(args, "format") == 0) {
    Seen c;
    if (waitCard(c, out)) {
      if (c.isA) cardFormat(c.a, out);
      done(c);
    }
  } else if (strcmp(args, "read") == 0) {
    cmdRead(out);
  } else if (strcmp(args, "list") == 0) {
    for (int i = 0; i < cardCount; i++) {
      out.printf("#%d %02x%02x%02x%02x\n", i, cards[i][0], cards[i][1], cards[i][2], cards[i][3]);
    }
  } else if (strncmp(args, "del ", 4) == 0) {
    int i = atoi(args + 4);
    if (i < 0 || i >= cardCount) {
      out.println("ERR bad index");
      return;
    }
    memmove(cards[i], cards[i + 1], (cardCount - i - 1) * 16);
    cardCount--;
    saveCards();
  } else if (strcmp(args, "clear") == 0) {
    cardCount = 0;
    prefs.remove("cards");
  } else if (strcmp(args, "unlock") == 0) {
    unlocked = true;
    unlockedAt = millis();
  } else {
    out.println("ERR usage: nfc [probe|add|format|list|del <n>|clear|read|unlock]");
    return;
  }
  out.println("OK");
}
