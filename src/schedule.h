#pragma once
#include <time.h>

#include "config.h"

// config.json以外から来た予定 (Googleカレンダーなど) をまとめて差し替える
#define MAX_EXT_ITEMS 64
void scheduleSetExternal(const Item* items, int n);

// その日の予定 (開始時刻順)。config.jsonの分と外部の分を合わせる
int scheduleOfDay(int ymd, const Item** out, int max);

// 次のミーティング (今日から1週間先まで見る)
const Item* nextMeeting(time_t now, int* ymd);

time_t itemStart(const Item* it, int ymd);
time_t itemEnd(const Item* it, int ymd);

// MTGの数分前に鳴らす
void reminderLoop();

// 通知中のMTG (無ければnullptr)
const Item* reminderActive(int* ymd);
