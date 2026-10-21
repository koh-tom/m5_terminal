#!/bin/sh
# fonts/*.vlw を作り直す。src に新しい文字列を足したら実行する
#   Roboto (Apache-2.0), Noto Sans/Serif CJK (OFL-1.1), JetBrainsMono Nerd Font (OFL-1.1)
set -e
cd "$(dirname "$0")/.."

ROBOTO=${ROBOTO:-/usr/share/fonts/TTF}
NOTO=${NOTO:-/usr/share/fonts/noto-cjk}
NERD=${NERD:-/usr/share/fonts/TTF/JetBrainsMonoNerdFontPropo-Regular.ttf}
mk="uv run -q tools/mkfont.py"

$mk "$ROBOTO/Roboto-Light.ttf" 124 --chars "0123456789:" -o fonts/clock.vlw
$mk "$ROBOTO/Roboto-Light.ttf" 84 --chars "0123456789:" -o fonts/num84.vlw
$mk "$ROBOTO/Roboto-Light.ttf" 40 --chars "0123456789:.-+D" -o fonts/num40.vlw
$mk "$ROBOTO/Roboto-Medium.ttf" 12 --ascii -o fonts/lat12.vlw
$mk "$NOTO/NotoSansCJK-Regular.ttc" 13 --ascii --src src -o fonts/jp13.vlw
$mk "$NOTO/NotoSansCJK-Regular.ttc" 16 --ascii --kana --jis1 --src src -o fonts/jp16.vlw
$mk "$NOTO/NotoSansCJK-Medium.ttc" 20 --ascii --kana --src src -o fonts/jp20.vlw
$mk "$NOTO/NotoSansCJK-Medium.ttc" 36 --chars "在室外出中不" -o fonts/jp36.vlw
$mk "$NOTO/NotoSerifCJK-Black.ttc" 24 --chars "活動限界まで再開待機停止中内部外部電源接続本日稼働" -o fonts/eva24.vlw

# QRカードのアイコン: github, globe, butterfly(bluesky), mail, flask, graduation-cap, link
$mk "$NERD" 30 --codes f09b,f0ac,e28e,f0e0,f0c3,f19d,f0c1 -o fonts/icon30.vlw
