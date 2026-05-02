#pragma once
#include <time.h>

void timeBegin();
void timeLoop();
void timeSet(time_t t);
bool timeValid();

// 20261021 みたいな形 (時刻が未設定なら0)
int todayYmd();
