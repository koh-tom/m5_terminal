#pragma once

constexpr int UI_FOOTER_H = 40;
constexpr int UI_FOOTER_Y = 240 - UI_FOOTER_H;

enum class Screen { Clock, Focus, Count };

void uiBegin();
void uiLoop();
void uiDirty();

Screen uiScreen();
void uiNextScreen();
void uiMoveScreen(int dir);

void uiShot();
