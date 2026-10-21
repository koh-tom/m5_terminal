#include "tilt.h"

#include <M5Unified.h>

static float ax, ay, az;
static bool upside = false;
static uint32_t changedAt = 0;
static uint32_t lastRead = 0;

void tiltBegin() {
  if (!M5.Imu.isEnabled()) Serial.println("[imu] none");
}

void tiltLoop() {
  if (!M5.Imu.isEnabled() || millis() - lastRead < 100) return;
  lastRead = millis();
  M5.Imu.update();
  M5.Imu.getAccel(&ax, &ay, &az);

  // 立てて置くとyが+1g。-0.6gより下なら逆さま、0.3gより上で戻す
  bool now = upside ? ay < 0.3f : ay < -0.6f;
  if (now != upside) {
    // 0.7秒続いたら確定 (持ち上げた時のぶれ対策)
    if (changedAt == 0) changedAt = millis();
    if (millis() - changedAt > 700) {
      upside = now;
      changedAt = 0;
    }
  } else {
    changedAt = 0;
  }
}

bool tiltUpsideDown() { return upside; }

void tiltAccel(float* x, float* y, float* z) {
  *x = ax;
  *y = ay;
  *z = az;
}
