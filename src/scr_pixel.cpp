// 横向きにした時のドット絵。出入りはドラクエの戦闘っぽく1マスずつ
#include "gfx.h"
#include "pixelart.h"
#include "screens.h"

namespace {
constexpr int TILE = 4;          // 剥がれる単位 (元のドット数)
constexpr uint32_t IN_MS = 700;
constexpr uint32_t OUT_DELAY = 700;
constexpr float GRAVITY = 1800;  // px/s^2

uint32_t hash(uint32_t i) {
  i ^= i >> 16;
  i *= 0x7feb352d;
  i ^= i >> 15;
  i *= 0x846ca68b;
  i ^= i >> 16;
  return i;
}

// タイル1枚を描く。flashなら白く光らせる
void tile(int tx, int ty, int ox, int oy, int s, uint16_t bg, bool flash) {
  int w = artWidth(), h = artHeight();
  for (int y = ty * TILE; y < min(h, (ty + 1) * TILE); y++) {
    for (int x = tx * TILE; x < min(w, (tx + 1) * TILE); x++) {
      uint16_t c = artPixel(x, y);
      if (c == bg) continue;
      canvas.fillRect(ox + x * s, oy + y * s, s, s, flash ? TFT_WHITE : c);
    }
  }
}
}  // namespace

uint32_t pixelAnimMs(bool leaving) {
  return leaving ? OUT_DELAY + 800 : IN_MS + 100;
}

void drawPixelArt(const Frame& f, int mode, uint32_t t) {
  int W = canvas.width(), H = canvas.height();
  if (!artAvailable()) return;
  int w = artWidth(), h = artHeight();
  int s = max(1, min(W / w, 240 / h));
  int ox = (W - w * s) / 2, oy = 24 + (240 - h * s) / 2;
  uint16_t bg = artPixel(0, 0);
  canvas.fillScreen(bg);

  int cols = (w + TILE - 1) / TILE, rows = (h + TILE - 1) / TILE;
  for (int ty = 0; ty < rows; ty++) {
    for (int tx = 0; tx < cols; tx++) {
      uint32_t r = hash(ty * cols + tx + 1);
      if (mode == 0) {
        tile(tx, ty, ox, oy, s, bg, false);
      } else if (mode == 1) {
        // ばらばらに出てくる。出た瞬間だけ白く光る
        uint32_t at = r % (IN_MS - 150);
        if (t < at) continue;
        tile(tx, ty, ox, oy, s, bg, t < at + 60);
      } else {
        // 光ってから剥がれて落ちる
        uint32_t at = r % OUT_DELAY;
        if (t < at) {
          tile(tx, ty, ox, oy, s, bg, false);
        } else if (t < at + 50) {
          tile(tx, ty, ox, oy, s, bg, true);
        } else {
          float dt = (t - at - 50) / 1000.0f;
          int dy = (int)(0.5f * GRAVITY * dt * dt);
          int dx = (int)(((int)(hash(r) % 121) - 60) * dt);
          if (oy + ty * TILE * s + dy < H) tile(tx, ty, ox + dx, oy + dy, s, bg, false);
        }
      }
    }
  }

  if (mode == 0) {
    char buf[8];
    snprintf(buf, sizeof(buf), "%02d:%02d", f.t.tm_hour, f.t.tm_min);
    text(buf, W / 2, H - 22, Font::Num40, rgb(230, 230, 255), middle_center);
  }
}
