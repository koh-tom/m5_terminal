#!/usr/bin/env python3
# /// script
# dependencies = ["pyserial", "pillow"]
# ///
"""画面キャプチャを png で保存する

    uv run tools/shot.py -o shot.png
"""
import argparse
import time

from PIL import Image

from port import open_port


def rgb565_to_rgb(data):
    out = bytearray()
    for i in range(0, len(data), 2):
        v = data[i] << 8 | data[i + 1]
        r, g, b = v >> 11 & 0x1F, v >> 5 & 0x3F, v & 0x1F
        out += bytes((r << 3 | r >> 2, g << 2 | g >> 4, b << 3 | b >> 2))
    return bytes(out)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("-p", "--port", default="/dev/ttyACM0")
    ap.add_argument("-o", "--out", default="shot.png")
    args = ap.parse_args()

    with open_port(args.port) as s:
        time.sleep(0.2)
        s.reset_input_buffer()
        s.write(b"shot\n")
        while True:
            line = s.readline().decode(errors="replace").strip()
            if not line:
                raise SystemExit("no response")
            if line.startswith("SHOT "):
                break
        w, h = map(int, line.split()[1:3])
        data = s.read(w * h * 2)
        if len(data) != w * h * 2:
            raise SystemExit(f"short read: {len(data)}")

    Image.frombytes("RGB", (w, h), rgb565_to_rgb(data)).save(args.out)
    print(args.out)


if __name__ == "__main__":
    main()
