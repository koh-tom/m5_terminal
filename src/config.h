#pragma once
#include <Arduino.h>

#define MAX_WIFI 3
#define MAX_DEADLINES 4
#define MAX_ITEMS 32
#define TITLE_LEN 48
#define MAX_CONFIG_SIZE 16384

struct WifiCred {
  char ssid[33];
  char pass[65];
};

struct Deadline {
  int ymd;
  char title[TITLE_LEN];
};

struct Item {
  int ymd;        // 単発の予定。毎週のは0
  uint8_t dows;   // 毎週の曜日 (bit0=日曜)
  int16_t start;  // 0時からの分
  int16_t end;    // 無ければ-1
  bool mtg;       // ミーティング扱いにするか
  char title[TITLE_LEN];
};

struct Config {
  WifiCred wifi[MAX_WIFI];
  int wifiCount = 0;
  char tz[32] = "JST-9";
  char ntp[64] = "ntp.nict.jp";

  int workMin = 50;
  int breakMin = 10;
  int remindMin = 5;
  bool pauseOnMtg = true;
  int volume = 0;  // 0で鳴らさない

  bool clockAnalog = false;
  bool focusEva = false;

  // 画面の明るさ (夜は暗くする)
  int brightDay = 120;
  int brightNight = 20;
  int nightFrom = 22 * 60;
  int nightTo = 7 * 60;

  Deadline deadlines[MAX_DEADLINES];
  int deadlineCount = 0;

  Item items[MAX_ITEMS];
  int itemCount = 0;
};

extern Config cfg;

bool loadConfig();

// 受け取ったjsonをチェックして保存し、読み直す
bool saveConfig(const char* json, size_t len);
