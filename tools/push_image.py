#!/usr/bin/env python3
# /// script
# dependencies = ["pyserial", "pillow", "numpy"]
# ///
"""ドット絵を横向き用の画像として書き込む

AIで作ったドット絵みたいに拡大されているものは --grid で元のドット数を指定すると
マスごとに色を拾い直してくっきりさせる。

    uv run tools/push_image.py art.png --grid 120
"""
import argparse
import struct
import time

import numpy as np
import serial
from PIL import Image

MAX = 240


def to_grid(im, n):
    # 各マスの真ん中あたりの色の中央値をとる (境目のにじみを避ける)
    a = np.asarray(im)
    sy, sx = a.shape[0] / n, a.shape[1] / n
    out = np.zeros((n, n, 3), np.uint8)
    for y in range(n):
        for x in range(n):
            c = a[int(y * sy + sy * 0.25) : int(y * sy + sy * 0.75) + 1,
                  int(x * sx + sx * 0.25) : int(x * sx + sx * 0.75) + 1].reshape(-1, 3)
            out[y, x] = np.median(c, axis=0)
    return Image.fromarray(out)


def encode(im):
    w, h = im.size
    out = bytearray(b"PX01" + struct.pack("<HH", w, h))
    for r, g, b in np.asarray(im).reshape(-1, 3).tolist():
        v = (r & 0xF8) << 8 | (g & 0xFC) << 3 | b >> 3
        out += struct.pack(">H", v)
    return bytes(out)


def send(port, cmd, data):
    with serial.Serial(port, 115200, timeout=3) as s:
        time.sleep(0.2)
        s.reset_input_buffer()
        s.write(f"{cmd} {len(data)}\n".encode())
        res = s.readline().decode(errors="replace").strip()
        if res != "READY":
            raise SystemExit(res or "no response")
        for i in range(0, len(data), 256):
            s.write(data[i : i + 256])
            s.flush()
            time.sleep(0.01)
        for _ in range(20):
            res = s.readline().decode(errors="replace").strip()
            if res.startswith(("OK", "ERR")):
                return res
    return "no response"


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("image")
    ap.add_argument("-p", "--port", default="/dev/ttyACM0")
    ap.add_argument("--grid", type=int, help="元のドット数 (正方形)")
    ap.add_argument("-o", "--preview", help="変換後の画像をpngで保存")
    args = ap.parse_args()

    im = Image.open(args.image).convert("RGB")
    if args.grid:
        im = to_grid(im, args.grid)
    if max(im.size) > MAX:
        im.thumbnail((MAX, MAX), Image.NEAREST)
    if args.preview:
        im.resize((im.width * 2, im.height * 2), Image.NEAREST).save(args.preview)

    print(f"{im.width}x{im.height}:", send(args.port, "image", encode(im)))


if __name__ == "__main__":
    main()
