#pragma once
#include <stddef.h>
#include <time.h>

void timeBegin();
void timeLoop();
void timeSet(time_t t);
bool timeValid();
time_t lastNtpSync();
void timeResync();

// 20261021 みたいな形 (時刻が未設定なら0)
int todayYmd();
int ymdAddDays(int ymd, int days);
int ymdWeekday(int ymd);  // 0=日
int ymdDiff(int from, int to);

// "2026-10-21 09:30:00"
void fmtTime(time_t t, char* buf, size_t n);
