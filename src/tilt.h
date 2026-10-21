#pragma once
#include <stdint.h>

// IMUで向きを見る (IMUが無い機種では常にUp)
//   Up: 普通に立てた状態 / Down: 上下逆さま / Left, Right: 横に90度倒した状態
enum class Orient : uint8_t { Up, Down, Left, Right };

void tiltBegin();
void tiltLoop();
Orient tiltOrient();
bool tiltUpsideDown();

// 最後に読んだ加速度 (g)
void tiltAccel(float* x, float* y, float* z);
