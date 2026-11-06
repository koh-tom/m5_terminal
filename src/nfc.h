#pragma once
#include <Arduino.h>

// Unit NFC (ST25R3916) を Port A で使う。無ければ何もしない
bool nfcBegin();
bool nfcReady();

// 今かざしてあるカードを1回読んで種類と番号を出す (確認用)
void nfcProbe(Print& out);
