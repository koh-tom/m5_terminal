#include "light.h"

#include <M5Unified.h>

namespace {
constexpr uint8_t ADDR = 0x23;
constexpr uint32_t FREQ = 400000;

constexpr uint8_t ALS_CONTR = 0x80;
constexpr uint8_t PS_CONTR = 0x81;
constexpr uint8_t PS_LED = 0x82;
constexpr uint8_t PS_N_PULSES = 0x83;
constexpr uint8_t PS_MEAS_RATE = 0x84;
constexpr uint8_t ALS_MEAS_RATE = 0x85;
constexpr uint8_t PART_ID = 0x86;
constexpr uint8_t ALS_DATA = 0x88;  // CH1 L/H, CH0 L/H
constexpr uint8_t PS_DATA = 0x8D;

bool ok = false;
int lux = -1;
int prox = 0;
uint32_t lastRead = 0;

bool wr(uint8_t reg, uint8_t v) { return M5.In_I2C.writeRegister8(ADDR, reg, v, FREQ); }
}  // namespace

bool lightBegin() {
  if (M5.getBoard() != m5::board_t::board_M5StackCoreS3) return false;
  uint8_t id = M5.In_I2C.readRegister8(ADDR, PART_ID, FREQ);
  if ((id >> 4) != 0x9) {
    Serial.printf("[light] not found (%02x)\n", id);
    return false;
  }
  ok = wr(ALS_CONTR, 0x01)            // ALS 有効, gain x1
       && wr(PS_LED, 0x7B)            // 60kHz, 100%, 50mA
       && wr(PS_N_PULSES, 0x08)
       && wr(PS_MEAS_RATE, 0x02)      // 100ms
       && wr(ALS_MEAS_RATE, 0x03)     // 100ms積分, 500ms周期
       && wr(PS_CONTR, 0x03);         // PS 有効
  Serial.printf("[light] %s\n", ok ? "ok" : "init failed");
  return ok;
}

void lightLoop() {
  if (!ok || millis() - lastRead < 200) return;
  lastRead = millis();

  uint8_t a[4];
  if (M5.In_I2C.readRegister(ADDR, ALS_DATA, a, 4, FREQ)) {
    int ch1 = a[0] | a[1] << 8;
    int ch0 = a[2] | a[3] << 8;
    // データシートの近似式 (gain1, 100ms)
    if (ch0 + ch1 == 0) {
      lux = 0;
    } else {
      float r = (float)ch1 / (ch0 + ch1);
      float l;
      if (r < 0.45f) l = 1.7743f * ch0 + 1.1059f * ch1;
      else if (r < 0.64f) l = 4.2785f * ch0 - 1.9548f * ch1;
      else if (r < 0.85f) l = 0.5926f * ch0 + 0.1185f * ch1;
      else l = 0;
      lux = (int)l;
    }
  }
  uint8_t p[2];
  if (M5.In_I2C.readRegister(ADDR, PS_DATA, p, 2, FREQ)) {
    prox = (p[0] | p[1] << 8) & 0x7FF;
  }
}

bool lightAvailable() { return ok; }
int lightLux() { return ok ? lux : -1; }
int lightProximity() { return prox; }
bool lightHover() { return ok && prox > 400; }
