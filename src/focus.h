#pragma once
#include <stdint.h>

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

// 今日の合計
uint32_t focusTodaySec();
int focusTodayCount();

// 直近7日の研究時間(秒)。out[6]が今日
void focusWeek(uint32_t out[7]);
