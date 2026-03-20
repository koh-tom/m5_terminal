#pragma once

enum class Screen { Clock, Focus, Count };

void uiBegin();
void uiLoop();
void uiDirty();
void uiNextScreen();
void uiShot();
