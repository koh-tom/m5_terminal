// 逆さまにした時のQRカード (来客・学会用)
#include "config.h"
#include "gfx.h"
#include "screens.h"
#include "ui.h"

namespace {

struct Icon {
  const char* name;
  const char* glyph;  // Nerd Fontのコードポイント (UTF-8)
  uint16_t bg;
  uint16_t fg;
};

const Icon ICONS[] = {
    {"github", "", col::TEXT, col::BG},
    {"bluesky", "", rgb(17, 133, 254), col::TEXT},
    {"web", "", col::ACCENT, col::BG},
    {"mail", "", col::AMBER, col::BG},
    {"lab", "", col::GREEN, col::BG},
    {"school", "", rgb(170, 130, 255), col::BG},
    {"link", "", col::SURFACE2, col::TEXT},
};

bool contains(const char* s, const char* part) { return strstr(s, part) != nullptr; }

const Icon& iconFor(const Link& l) {
  for (const Icon& ic : ICONS) {
    if (strcmp(l.icon, ic.name) == 0) return ic;
  }
  if (contains(l.url, "://github.com/")) return ICONS[0];
  if (contains(l.url, "://bsky.app/")) return ICONS[1];
  return ICONS[2];
}

// URLから表示用の短い文字を作る
//   github.com/koh-tom → @koh-tom、bsky.app/profile/enje.dev → @enje.dev
void subFor(const Link& l, char* buf, size_t n) {
  if (l.sub[0]) {
    strlcpy(buf, l.sub, n);
    return;
  }
  const char* p = strstr(l.url, "://");
  p = p ? p + 3 : l.url;
  if (strncmp(p, "www.", 4) == 0) p += 4;

  if (strncmp(p, "github.com/", 11) == 0 || strncmp(p, "bsky.app/profile/", 17) == 0) {
    const char* last = strrchr(p, '/');
    if (last && last[1]) {
      snprintf(buf, n, "@%s", last + 1);
      return;
    }
  }
  strlcpy(buf, p, n);
  size_t len = strlen(buf);
  if (len > 0 && buf[len - 1] == '/') buf[len - 1] = '\0';
}

}  // namespace

void drawCard(int index, float elapsed) {
  canvas.fillScreen(col::BG);
  if (cfg.linkCount == 0) {
    text("config.json に card.links を書いてください", UI_W / 2, UI_H / 2, Font::Jp13, col::SUB, middle_center);
    return;
  }
  const Link& l = cfg.links[index % cfg.linkCount];
  const Icon& ic = iconFor(l);

  // QRは白いカードの上に (読み取り用の余白込み)
  roundRect(12, 20, 200, 200, 18, TFT_WHITE);
  canvas.qrcode(l.url, 26, 34, 172, 2);

  const int cx = 266, rw = 92;
  if (cfg.cardName[0]) textFit(cfg.cardName, cx, 34, rw, Font::Jp13, col::MUTED, middle_center);

  canvas.fillSmoothCircle(cx, 84, 30, ic.bg);
  text(ic.glyph, cx, 85, Font::Icon30, ic.fg, middle_center);

  textFit(l.label, cx, 134, rw, Font::Jp20, col::TEXT, middle_center);
  char sub[64];
  subFor(l, sub, sizeof(sub));
  textFit(sub, cx, 158, rw, Font::Lat12, col::SUB, middle_center);

  // ページ (今のは長いピル) と、次に切り替わるまでのバー
  int n = cfg.linkCount;
  int total = n * 6 + (n - 1) * 6 + 10;
  int x = cx - total / 2;
  for (int i = 0; i < n; i++) {
    bool cur = i == index % n;
    pill(x, 198, cur ? 16 : 6, 6, cur ? col::ACCENT : col::SURFACE2);
    x += (cur ? 16 : 6) + 6;
  }
  if (n > 1) {
    pill(cx - 30, 214, 60, 2, col::SURFACE2);
    pill(cx - 30, 214, max(2, (int)(60 * elapsed)), 2, col::SUB);
  }
}
