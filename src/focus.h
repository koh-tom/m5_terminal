#pragma once
#include <stdint.h>
#include <time.h>

// ポモドーロ (研究 / 休憩)
enum class Phase : uint8_t { Work, Break };
enum class RunState : uint8_t { Idle, Running, Paused };

void focusBegin();
void focusLoop();

void focusStart();
void focusPause();
void focusReset();
void focusToggle();

Phase focusPhase();
RunState focusState();
uint32_t focusRemainMs();
uint32_t focusLengthMs();
bool focusLongBreak();

// 今日の合計
uint32_t focusTodaySec();
int focusTodayCount();

// PCとの同期用。focus.csv の after より後に始まった区間を1件ずつ渡す
typedef void (*FocusSessionFn)(time_t start, time_t end, unsigned long sec, bool done, void* arg);
void focusEach(time_t after, FocusSessionFn fn, void* arg);

// 直近7日の研究時間(秒)。out[6]が今日
void focusWeek(uint32_t out[7]);
