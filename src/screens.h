#pragma once
#include <time.h>

// 1フレーム分の時刻
struct Frame {
  time_t now;
  struct tm t;
  int today;  // 0なら時刻未設定
};

void drawClock(const Frame& f, bool analog);
void drawAgenda(const Frame& f);
void drawFocus(const Frame& f);
void drawFocusFooter();
void drawFocusRing(const Frame& f);
void drawFocusDial(const Frame& f);
void drawFocusWater(const Frame& f);
void drawFocusGrid(const Frame& f);
void drawFocusHourglass(const Frame& f);
void drawEva(const Frame& f);
void drawEvaFooter();
void drawPresence(const Frame& f);
void drawPresenceFooter();
void drawStats(const Frame& f);
void drawCard(int index, float elapsed);  // elapsed: 次のリンクまでの進み 0-1

// 時計と予定で使う
struct Deadline;
const Deadline* nearestDeadline(int today, int* days);
uint16_t deadlineColor(int days);
