#pragma once

void inputBegin();
void inputLoop();

// コンソールからも呼べるようにしておく
void inputButton(int idx, bool hold);
void inputTap(int x, int y);
void inputLongPress(int x, int y);
void inputFlick(int dir);
