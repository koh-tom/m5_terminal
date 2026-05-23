#pragma once

constexpr int UI_HEADER_H = 24;
constexpr int UI_TAB_W = 56;
constexpr int UI_FOOTER_H = 40;
constexpr int UI_FOOTER_Y = 240 - UI_FOOTER_H;

enum class Screen { Clock, Focus, Stats, Count };

void uiBegin();
void uiLoop();
void uiDirty();

Screen uiScreen();
void uiSetScreen(Screen s);
void uiNextScreen();
void uiMoveScreen(int dir);

void uiShot();
