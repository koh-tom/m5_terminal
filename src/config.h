#pragma once
#include <Arduino.h>

#define MAX_WIFI 3
#define MAX_DEADLINES 4
#define TITLE_LEN 48

struct WifiCred {
  char ssid[33];
  char pass[65];
};

struct Deadline {
  int ymd;
  char title[TITLE_LEN];
};

struct Config {
  WifiCred wifi[MAX_WIFI];
  int wifiCount = 0;
  char tz[32] = "JST-9";
  char ntp[64] = "ntp.nict.jp";

  int workMin = 50;
  int breakMin = 10;

  Deadline deadlines[MAX_DEADLINES];
  int deadlineCount = 0;
};

extern Config cfg;

bool loadConfig();
