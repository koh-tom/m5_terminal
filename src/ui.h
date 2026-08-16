#pragma once

constexpr int UI_HEADER_H = 24;
constexpr int UI_TAB_W = 56;
constexpr int UI_FOOTER_H = 40;
constexpr int UI_FOOTER_Y = 240 - UI_FOOTER_H;

enum class Screen { Clock, Focus, Presence, Stats, Count };

void uiBegin();
void uiLoop();
void uiDirty();
void uiWake();  // 夜でも少しの間明るくする

Screen uiScreen();
void uiSetScreen(Screen s);
void uiNextScreen();
void uiMoveScreen(int dir);

// 少しの間だけ出すメッセージ
void uiToast(const char* msg);

void uiShot();
