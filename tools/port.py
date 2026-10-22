"""tools/*.py 共通: シリアルポートを開く

labterm-presence (PC側のdaemon) がポートを使っている時は、
`labterm release -- <コマンド>` で一時的に空けてもらう必要があるので、その案内を出す。
"""
import shlex
import sys

import serial


def open_port(port, timeout=3):
    try:
        return serial.Serial(port, 115200, timeout=timeout, exclusive=True)
    except serial.SerialException as e:
        msg = str(e).lower()
        if "busy" in msg or "lock" in msg or "temporarily unavailable" in msg:
            cmd = "uv run " + " ".join(shlex.quote(a) for a in sys.argv)
            raise SystemExit(
                f"{port} は使用中です。labterm-presence が動いているなら次で実行してください:\n"
                f"  labterm release -- {cmd}"
            ) from None
        raise
