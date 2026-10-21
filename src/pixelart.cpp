#include "pixelart.h"

#include <Arduino.h>
#include <LittleFS.h>

static const char* PATH = "/pixel.bin";
static uint16_t* pixels = nullptr;
static int w = 0, h = 0;

static bool validHeader(const uint8_t* d, size_t len, int* ow, int* oh) {
  if (len < 8 || memcmp(d, "PX01", 4) != 0) return false;
  int iw = d[4] | d[5] << 8, ih = d[6] | d[7] << 8;
  if (iw <= 0 || ih <= 0 || iw > ART_MAX || ih > ART_MAX) return false;
  if (len != 8 + (size_t)iw * ih * 2) return false;
  *ow = iw;
  *oh = ih;
  return true;
}

static void load(const uint8_t* d, int iw, int ih) {
  free(pixels);
  pixels = (uint16_t*)ps_malloc(iw * ih * 2);
  if (!pixels) pixels = (uint16_t*)malloc(iw * ih * 2);
  if (!pixels) {
    w = h = 0;
    return;
  }
  for (int i = 0; i < iw * ih; i++) pixels[i] = d[8 + i * 2] << 8 | d[9 + i * 2];
  w = iw;
  h = ih;
}

bool artBegin() {
  File f = LittleFS.open(PATH, FILE_READ);
  if (!f) return false;
  size_t len = f.size();
  if (len > 8 + ART_MAX * ART_MAX * 2) {
    f.close();
    return false;
  }
  uint8_t* buf = (uint8_t*)ps_malloc(len);
  if (!buf) buf = (uint8_t*)malloc(len);
  bool ok = buf && f.read(buf, len) == len;
  f.close();
  int iw, ih;
  if (ok && validHeader(buf, len, &iw, &ih)) {
    load(buf, iw, ih);
    Serial.printf("[art] %dx%d\n", w, h);
  }
  free(buf);
  return w > 0;
}

bool artAvailable() { return pixels && w > 0; }
int artWidth() { return w; }
int artHeight() { return h; }
uint16_t artPixel(int x, int y) { return pixels[y * w + x]; }

bool artSave(const uint8_t* data, size_t len) {
  int iw, ih;
  if (!validHeader(data, len, &iw, &ih)) return false;
  File f = LittleFS.open("/pixel.tmp", FILE_WRITE);
  if (!f) return false;
  bool ok = f.write(data, len) == len;
  f.close();
  if (!ok) {
    LittleFS.remove("/pixel.tmp");
    return false;
  }
  LittleFS.remove(PATH);
  if (!LittleFS.rename("/pixel.tmp", PATH)) return false;
  load(data, iw, ih);
  return artAvailable();
}

void artClear() {
  LittleFS.remove(PATH);
  free(pixels);
  pixels = nullptr;
  w = h = 0;
}
