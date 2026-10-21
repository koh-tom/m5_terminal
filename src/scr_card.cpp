// 逆さまにした時のQRカード (来客・学会用)
#include "config.h"
#include "gfx.h"
#include "screens.h"
#include "ui.h"

namespace {
constexpr uint16_t PAPER = rgb(250, 250, 248);
constexpr uint16_t INK = rgb(20, 22, 28);
constexpr uint16_t GRAY = rgb(120, 124, 132);

// URLを幅に合わせて折り返す
void wrapUrl(const char* url, int x, int y, int maxW) {
  if (strncmp(url, "https://", 8) == 0) url += 8;
  else if (strncmp(url, "http://", 7) == 0) url += 7;

  char line[64];
  size_t n = 0;
  for (const char* p = url; *p && y < UI_H - 30; p++) {
    line[n++] = *p;
    line[n] = '\0';
    if (n >= sizeof(line) - 1 || textWidth(line, Font::Lat12) > maxW) {
      line[n - 1] = '\0';
      text(line, x, y, Font::Lat12, GRAY, top_left);
      y += 16;
      n = 0;
      p--;
    }
  }
  if (n) text(line, x, y, Font::Lat12, GRAY, top_left);
}
}  // namespace

void drawCard(int index) {
  canvas.fillScreen(PAPER);
  if (cfg.linkCount == 0) {
    text("config.json に card.links を書いてください", UI_W / 2, UI_H / 2, Font::Jp13, GRAY, middle_center);
    return;
  }
  const Link& l = cfg.links[index % cfg.linkCount];

  const int qs = 200, qx = 12, qy = (UI_H - qs) / 2;
  canvas.fillRect(qx, qy, qs, qs, TFT_WHITE);
  canvas.qrcode(l.url, qx, qy, qs, 2);

  const int rx = qx + qs + 10, rw = UI_W - rx - 10;
  int y = 24;
  if (cfg.cardName[0]) {
    textFit(cfg.cardName, rx, y, rw, Font::Jp16, INK, top_left);
    y += 28;
  }
  textFit(l.label, rx, y, rw, Font::Jp20, INK, top_left);
  wrapUrl(l.url, rx, y + 32, rw);

  // ページの点
  for (int i = 0; i < cfg.linkCount; i++) {
    int cx = rx + 6 + i * 14;
    if (i == index % cfg.linkCount) {
      canvas.fillSmoothCircle(cx, UI_H - 22, 4, INK);
    } else {
      canvas.fillSmoothCircle(cx, UI_H - 22, 4, rgb(210, 212, 216));
    }
  }
}
