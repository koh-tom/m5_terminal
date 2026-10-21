#pragma once

// IMUで上下逆さまになったかを見る (IMUが無い機種では常にfalse)
void tiltBegin();
void tiltLoop();
bool tiltUpsideDown();

// 最後に読んだ加速度 (g)
void tiltAccel(float* x, float* y, float* z);
