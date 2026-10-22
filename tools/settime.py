#!/usr/bin/env python3
# /// script
# dependencies = ["pyserial"]
# ///
"""PCの時刻を labterm に書き込む (wifiが無い環境用)

    uv run tools/settime.py -p /dev/ttyACM0
"""
import argparse
import time

from port import open_port



def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("-p", "--port", default="/dev/ttyACM0")
    args = ap.parse_args()

    with open_port(args.port, timeout=2) as s:
        time.sleep(0.2)
        s.reset_input_buffer()
        s.write(f"time {int(time.time())}\n".encode())
        for _ in range(10):
            res = s.readline().decode(errors="replace").strip()
            if res.startswith(("OK", "ERR")):
                print(res)
                return
        print("no response")


if __name__ == "__main__":
    main()
