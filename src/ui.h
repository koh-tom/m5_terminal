#pragma once

constexpr int UI_W = 320;
constexpr int UI_H = 240;
constexpr int UI_HEADER_H = 28;
constexpr int UI_TAB_W = 64;
constexpr int UI_FOOTER_H = 44;
constexpr int UI_FOOTER_Y = UI_H - UI_FOOTER_H;

enum class FocusStyle { Normal, Ring, Dial, Water, Grid, Hourglass, Eva, Count };

enum class Screen { Clock, Agenda, Focus, Presence, Stats, Count };

void uiBegin();
void uiLoop();
void uiDirty();
void uiWake();  // 夜でも少しの間明るくする

Screen uiScreen();
void uiSetScreen(Screen s);
void uiNextScreen();
void uiMoveScreen(int dir);

// 下のボタンがある画面か (集中と在室だけ)
bool uiHasFooter();

// 時計: デジタル/アナログ、集中: FocusStyleを順番に
void uiToggleStyle();

// QRカード (逆さまにした時)
bool uiCardActive();
void uiCardNext();
void uiCardForce(bool on);  // IMUが無い機種やテスト用

// 少しの間だけ出すメッセージ
void uiToast(const char* msg);

void uiShot();
