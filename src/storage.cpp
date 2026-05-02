#include "storage.h"

#include <M5Unified.h>
#include <SD.h>
#include <SPI.h>

static bool ready = false;

bool sdBegin() {
  int cs = M5.getPin(m5::pin_name_t::sd_spi_cs);
  int sclk = M5.getPin(m5::pin_name_t::sd_spi_sclk);
  int mosi = M5.getPin(m5::pin_name_t::sd_spi_mosi);
  int miso = M5.getPin(m5::pin_name_t::sd_spi_miso);
  if (cs < 0) return false;

  SPI.begin(sclk, miso, mosi, cs);
  ready = SD.begin(cs, SPI, 20000000);
  if (ready) {
    SD.mkdir("/labterm");
  }
  Serial.printf("[sd] %s\n", ready ? "ok" : "not found");
  return ready;
}

bool sdReady() { return ready; }

bool sdAppend(const char* path, const char* line, const char* header) {
  if (!ready) return false;
  bool exists = SD.exists(path);
  File f = SD.open(path, FILE_APPEND);
  if (!f) {
    Serial.printf("[sd] open failed: %s\n", path);
    return false;
  }
  if (!exists && header) f.println(header);
  f.println(line);
  f.close();
  return true;
}
