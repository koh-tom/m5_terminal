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
