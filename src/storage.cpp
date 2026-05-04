#include "storage.h"

#include <LittleFS.h>
#include <M5Unified.h>
#include <SD.h>
#include <SPI.h>

static bool sdOk = false;

static bool mountSd() {
  int cs = M5.getPin(m5::pin_name_t::sd_spi_cs);
  int sclk = M5.getPin(m5::pin_name_t::sd_spi_sclk);
  int mosi = M5.getPin(m5::pin_name_t::sd_spi_mosi);
  int miso = M5.getPin(m5::pin_name_t::sd_spi_miso);
  if (cs < 0) return false;

  SPI.begin(sclk, miso, mosi, cs);
  return SD.begin(cs, SPI, 20000000);
}

bool storageBegin() {
  if (!LittleFS.begin(true)) {
    Serial.println("[fs] littlefs mount failed");
  }
  sdOk = mountSd();
  logFS().mkdir("/labterm");
  Serial.printf("[fs] log -> %s\n", sdOk ? "sd" : "flash");
  return sdOk;
}

bool sdReady() { return sdOk; }

fs::FS& logFS() {
  if (sdOk) return SD;
  return LittleFS;
}

bool logAppend(const char* path, const char* line, const char* header) {
  // exists()はlittlefsだとエラーログが出るのでsizeで見る
  File f = logFS().open(path, FILE_APPEND, true);
  if (!f) {
    Serial.printf("[fs] open failed: %s\n", path);
    return false;
  }
  if (f.size() == 0 && header) f.println(header);
  f.println(line);
  f.close();
  return true;
}
