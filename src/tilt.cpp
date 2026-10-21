#include "tilt.h"

#include <M5Unified.h>

static float ax, ay, az;
static Orient orient = Orient::Up;
static Orient pending = Orient::Up;
static uint32_t changedAt = 0;
static uint32_t lastRead = 0;

void tiltBegin() {
  if (!M5.Imu.isEnabled()) Serial.println("[imu] none");
}

// 立てて置くとyが+1g。はっきり傾いている時だけ向きを決める (寝かせた時などは前のまま)
static Orient raw(Orient cur) {
  if (ay > 0.6f) return Orient::Up;
  if (ay < -0.6f) return Orient::Down;
  if (ax > 0.6f) return Orient::Right;
  if (ax < -0.6f) return Orient::Left;
  return cur;
}

void tiltLoop() {
  if (!M5.Imu.isEnabled() || millis() - lastRead < 100) return;
  lastRead = millis();
  M5.Imu.update();
  M5.Imu.getAccel(&ax, &ay, &az);

  Orient now = raw(orient);
  if (now == orient) {
    changedAt = 0;
    return;
  }
  // 0.7秒続いたら確定 (持ち上げた時のぶれ対策)
  if (changedAt == 0 || now != pending) {
    pending = now;
    changedAt = millis();
  } else if (millis() - changedAt > 700) {
    orient = now;
    changedAt = 0;
  }
}

Orient tiltOrient() { return orient; }
bool tiltUpsideDown() { return orient == Orient::Down; }

void tiltAccel(float* x, float* y, float* z) {
  *x = ax;
  *y = ay;
  *z = az;
}
