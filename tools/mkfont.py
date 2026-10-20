#!/usr/bin/env python3
# /// script
# dependencies = ["pillow", "fonttools"]
# ///
"""TTF/OTF から LovyanGFX 用の vlw フォントを作る

    uv run tools/mkfont.py Roboto-Light.ttf 120 --chars "0123456789:" -o fonts/clock.vlw
    uv run tools/mkfont.py NotoSansCJK-Regular.ttc 16 --ascii --kana --jis1 --src src -o fonts/jp16.vlw
"""
import argparse
import pathlib
import re
import struct

from fontTools.ttLib import TTFont
from PIL import ImageFont


def jis1_chars():
    # JIS第1水準 (EUC-JPの0xB0A1-0xCFD3)
    out = []
    for hi in range(0xB0, 0xD0):
        for lo in range(0xA1, 0xFF):
            try:
                out.append(bytes([hi, lo]).decode("euc_jp"))
            except UnicodeDecodeError:
                pass
    return out


def kana_chars():
    out = [chr(c) for c in range(0x3041, 0x3097)]  # ひらがな
    out += [chr(c) for c in range(0x30A1, 0x30FB)]  # カタカナ
    out += list("ー・、。「」『』（）！？：〜…―→←↑↓○●◎×")
    return out


def src_chars(src):
    # ソース中の文字列リテラルに出てくる文字
    out = set()
    for p in pathlib.Path(src).rglob("*.cpp"):
        for lit in re.findall(r'"((?:[^"\\]|\\.)*)"', p.read_text(encoding="utf-8")):
            out.update(lit)
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("font")
    ap.add_argument("size", type=int)
    ap.add_argument("-o", "--out", required=True)
    ap.add_argument("--index", type=int, default=0, help="ttcの何番目か (NotoSansCJKのJPは0)")
    ap.add_argument("--chars", default="")
    ap.add_argument("--ascii", action="store_true")
    ap.add_argument("--kana", action="store_true")
    ap.add_argument("--jis1", action="store_true")
    ap.add_argument("--src", help="このディレクトリのソースに出てくる文字を入れる")
    args = ap.parse_args()

    chars = set(args.chars)
    if args.ascii:
        chars.update(chr(c) for c in range(0x20, 0x7F))
    if args.kana:
        chars.update(kana_chars())
    if args.jis1:
        chars.update(jis1_chars())
    if args.src:
        chars.update(src_chars(args.src))

    cmap = TTFont(args.font, fontNumber=args.index).getBestCmap()
    font = ImageFont.truetype(args.font, args.size, index=args.index)
    ascent, descent = font.getmetrics()

    glyphs = []
    for ch in sorted(c for c in chars if ord(c) <= 0xFFFF and ord(c) in cmap):
        adv = round(font.getlength(ch))
        if ch == " ":
            glyphs.append((ord(ch), 0, 0, adv, 0, 0, b""))
            continue
        mask, (ox, oy) = font.getmask2(ch, mode="L", anchor="ls")
        w, h = mask.size
        if w > 255 or adv > 255 or not -128 <= ox <= 127:
            raise SystemExit(f"glyph too large: {ch!r}")
        glyphs.append((ord(ch), h, w, adv, -oy, ox, bytes(mask)))

    out = bytearray(struct.pack(">6i", len(glyphs), 11, args.size, 0, ascent, descent))
    for code, h, w, adv, dy, dx, _ in glyphs:
        out += struct.pack(">7i", code, h, w, adv, dy, dx, 0)
    for g in glyphs:
        out += g[6]

    pathlib.Path(args.out).write_bytes(out)
    print(f"{args.out}: {len(glyphs)} glyphs, {len(out) // 1024} KB")


if __name__ == "__main__":
    main()
