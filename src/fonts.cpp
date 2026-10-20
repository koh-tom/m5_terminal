#include "fonts.h"

#define EMBED(name)                                                       \
  extern const uint8_t name##_start[] asm("_binary_fonts_" #name "_vlw_start"); \
  extern const uint8_t name##_end[] asm("_binary_fonts_" #name "_vlw_end");

EMBED(clock)
EMBED(num84)
EMBED(num40)
EMBED(lat12)
EMBED(jp13)
EMBED(jp16)
EMBED(jp20)
EMBED(jp36)
EMBED(eva24)

struct Entry {
  const uint8_t* start;
  const uint8_t* end;
  lgfx::PointerWrapper data;
  lgfx::VLWfont vlw;
};

static Entry entries[(int)Font::Count] = {
    {clock_start, clock_end}, {num84_start, num84_end}, {num40_start, num40_end},
    {lat12_start, lat12_end}, {jp13_start, jp13_end},   {jp16_start, jp16_end},
    {jp20_start, jp20_end},   {jp36_start, jp36_end},   {eva24_start, eva24_end},
};

void fontsBegin() {
  for (Entry& e : entries) {
    e.data.set(e.start, e.end - e.start);
    if (!e.vlw.loadFont(&e.data)) Serial.println("[font] load failed");
  }
}

const lgfx::IFont* font(Font f) {
  return &entries[(int)f].vlw;
}

// UTF-8を1文字ずつ読む
static uint32_t nextChar(const char*& s) {
  uint8_t c = *s++;
  if (c < 0x80) return c;
  int n = c >= 0xF0 ? 3 : c >= 0xE0 ? 2 : 1;
  uint32_t cp = c & (0x3F >> n);
  while (n-- && (*s & 0xC0) == 0x80) cp = (cp << 6) | (*s++ & 0x3F);
  return cp;
}

bool fontHas(Font f, const char* utf8) {
  const lgfx::VLWfont& v = entries[(int)f].vlw;
  while (*utf8) {
    uint32_t cp = nextChar(utf8);
    uint16_t idx;
    if (cp == ' ') continue;
    if (cp > 0xFFFF || !v.getUnicodeIndex(cp, &idx)) return false;
  }
  return true;
}
