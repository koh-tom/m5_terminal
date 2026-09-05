#!/usr/bin/env python3
# /// script
# dependencies = ["pyserial"]
# ///
"""config.json をUSB経由で書き込む (ログは消えない)

    uv run tools/push_config.py data/config.json
"""
import argparse
import json
import time

import serial


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("file")
    ap.add_argument("-p", "--port", default="/dev/ttyACM0")
    args = ap.parse_args()

    with open(args.file, "rb") as f:
        data = f.read()
    json.loads(data)  # 壊れてたらここで止める
    if len(data) > 16384:
        raise SystemExit("config.json is too large (max 16KB)")

    with serial.Serial(args.port, 115200, timeout=3) as s:
        time.sleep(0.2)
        s.reset_input_buffer()
        s.write(f"config {len(data)}\n".encode())
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
                print(res)
                return
        print("no response")


if __name__ == "__main__":
    main()
