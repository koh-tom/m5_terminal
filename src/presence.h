#pragma once
#include <stdint.h>
#include <time.h>

// 在室状態
enum class Presence : uint8_t { Out, In, Away };

void presenceBegin();
void presenceLoop();

void presenceSet(Presence p);
Presence presenceGet();
time_t presenceSince();      // 今の状態になった時刻
time_t presenceArrived();    // 今日最初に入室した時刻 (無ければ0)
uint32_t presenceTodaySec();  // 今日の在室時間
