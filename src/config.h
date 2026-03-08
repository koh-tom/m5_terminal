#pragma once
#include <Arduino.h>

#define MAX_WIFI 3

struct WifiCred {
  char ssid[33];
  char pass[65];
};

struct Config {
  WifiCred wifi[MAX_WIFI];
  int wifiCount = 0;
  char tz[32] = "JST-9";
  char ntp[64] = "ntp.nict.jp";
};

extern Config cfg;

bool loadConfig();
