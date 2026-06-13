#pragma once
#include <time.h>

#include "config.h"

// その日の予定 (開始時刻順)
int scheduleOfDay(int ymd, const Item** out, int max);

// 次のミーティング (今日から1週間先まで見る)
const Item* nextMeeting(time_t now, int* ymd);

time_t itemStart(const Item* it, int ymd);
time_t itemEnd(const Item* it, int ymd);
