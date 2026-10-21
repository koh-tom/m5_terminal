#pragma once
#include <stdint.h>

// CoreS3の照度/近接センサー (LTR-553ALS)。無い機種では何もしない
bool lightBegin();
void lightLoop();
bool lightAvailable();

float lightLux();     // だいたいのlux (-1なら不明)
int lightProximity(); // 0-2047、近いほど大きい
bool lightHover();    // 手をかざしている
