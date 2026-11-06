#include "nfc.h"

#include <M5UnitUnified.h>
#include <M5UnitUnifiedNFC.h>
#include <M5Unified.h>

#include <vector>

namespace {
m5::unit::UnitUnified units;
m5::unit::UnitNFC unit{};
m5::nfc::NFCLayerA nfcA{unit};
m5::nfc::NFCLayerF nfcF{unit};
bool ready = false;
}  // namespace

bool nfcBegin() {
  M5.Ex_I2C.begin();
  if (!M5.Ex_I2C.scanID(0x50)) {
    Serial.println("[nfc] not found");
    return false;
  }
  ready = units.add(unit, M5.Ex_I2C) && units.begin();
  Serial.printf("[nfc] %s\n", ready ? "ready" : "init failed");
  return ready;
}

bool nfcReady() { return ready; }

void nfcProbe(Print& out) {
  if (!ready) {
    out.println("nfc not ready");
    return;
  }
  units.update();

  // MIFARE / NTAG など (NFC-A)
  std::vector<m5::nfc::a::PICC> a;
  if (nfcA.detect(a)) {
    for (auto& p : a) {
      nfcA.identify(p);
      out.printf("A %s uid=%s user=%u total=%u\n", p.typeAsString().c_str(), p.uidAsString().c_str(),
                 (unsigned)p.userAreaSize(), (unsigned)p.totalSize());
    }
    nfcA.deactivate();
    return;
  }

  // Suica・学生証など (FeliCa)
  std::vector<m5::nfc::f::PICC> f;
  if (nfcF.detect(f)) {
    for (auto& p : f) {
      out.printf("F %s idm=%s pmm=%s\n", p.typeAsString().c_str(), p.idmAsString().c_str(),
                 p.pmmAsString().c_str());
    }
    return;
  }
  out.println("no card");
}
