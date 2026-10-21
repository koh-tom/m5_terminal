# labterm

M5Stack CoreS3 を研究室の机に置くチェックイン端末にしたもの。
時計 / 予定 / 締切カウントダウン / ポモドーロ / 在室ログ。

Core2 と Basic でも動く (Basic は物理ボタン、RTC が無いので wifi 必須)。

## 画面

| タブ | 内容 |
| --- | --- |
| 時計 | 大きい時刻 + 日付 + 一番近い締切。長押しでアナログ時計に切り替え |
| 予定 | 締切 (D-n) と、今日から1週間の予定。次のMTGまでの時間 |
| 集中 | 研究 50分 / 休憩 10分 のタイマー。長押しで表示を切り替え (下参照) |
| 在室 | 入室 / 外出 / 退室 |
| 記録 | 今日の目標の達成率、連続日数、直近7日の研究時間グラフ (目標を超えた日は緑) |

- 集中の表示: ふつう / リング / タイマー (残りが扇形で減る) / 水 (下からたまる) / マス (1分1マス) / 砂時計 / エヴァ
- 端末を上下逆さまにすると画面が180度回って QR カードになる (GitHub やポートフォリオのリンク)。8秒ごと/タップで次のリンク。戻すと元の画面。Basic は A長押し
- 下のボタンは集中と在室の画面だけ。集中: 開始 / 停止 / リセット、在室: 入室 / 外出 / 退室
- 画面切り替え: 上のタブをタップ、左右スワイプ
- Basic: ボタンが無い画面では A/C で画面送り、B長押しで次の画面、C長押しで表示切り替え
- 集中画面はタイマーの所をタップで開始/一時停止
- 研究中は集中タブに残り時間が赤で出る
- MTG の5分前にヘッダがオレンジになる。始まったらタイマーは自動で一時停止
- 研究を開始すると自動で入室扱いになる
- 夜 (22:00-7:00) か、部屋の電気を消すと画面を暗くする (CoreS3の照度センサー)。触るか手をかざすと30秒だけ明るくなる
- 4回ごとに長めの休憩 (20分)
- 退室し忘れたら朝4時に自動で退室
- 音はデフォルトで鳴らない (`volume` で設定)

## 使い方

```sh
cp data/config.example.json data/config.json   # 中身を編集
pio run -t upload
uv run tools/push_config.py data/config.json
uv run tools/settime.py   # wifi が無いとき。RTC に書くので一回でいい
```

SD カードがあれば `/labterm/config.json` を置いてもいい (SD 優先)。
`pio run -t uploadfs` でも書けるが、内蔵flashのログが消えるので push_config.py の方がいい。

### config.json

| キー | 説明 |
| --- | --- |
| `wifi` | `[{ "ssid", "pass" }]` 3つまで。時刻合わせの時だけつなぐ |
| `tz`, `ntp` | タイムゾーン (POSIX形式) と NTP サーバ |
| `pomodoro` | `{ "work": 50, "break": 10, "long_break": 20, "long_every": 4, "goal_min": 240 }` (分) |
| `remind_min` | MTG の何分前に知らせるか。0 で無効 |
| `pause_on_mtg` | MTG 開始でタイマーを止めるか |
| `volume` | 0-255。0 で鳴らさない (デフォルト) |
| `clock` | `"digital"` / `"analog"` (長押しで変えた方が優先) |
| `focus_style` | `"normal"` / `"ring"` / `"dial"` / `"water"` / `"grid"` / `"hourglass"` / `"eva"` |
| `brightness` | `{ "day", "night", "night_from", "night_to", "dark_lux" }` dark_lux 以下なら暗くする (-1で無効) |
| `auto_out` | `"4:00"` 自動で退室する時刻 |
| `card` | `{ "name", "links": [{ "label", "url", "sub", "icon" }] }` QRカード。http(s) のみ、4つまで<br>`icon`: github / bluesky / web / mail / lab / school / link (省略時はURLから判定) |
| `deadlines` | `[{ "title", "date": "2026-10-27" }]` |
| `schedule` | 毎週: `{ "dow": "mon,wed", "start", "end", "title", "mtg" }`<br>単発: `{ "date": "2026-10-23", "start", ... }` |

`mtg: true` の予定が「次のMTG」の対象になる。

## ログ

SD があれば SD、無ければ内蔵flash の `/labterm/` に csv で書く。

- `focus.csv` — 研究した区間 (`start,end,sec,done`)
- `daily.csv` — 日ごとの合計 (`date,sec,count`)
- `presence.csv` — 入室/外出/退室 (`time,state`)

USB シリアル (115200) で `cat /labterm/focus.csv` で読める。

## シリアルコマンド

| コマンド | |
| --- | --- |
| `status` | 時刻、SD、NTP、今日の合計など |
| `time <unix秒>` | 時刻合わせ (settime.py が使う) |
| `config <bytes>` | config.json 受信 (push_config.py が使う) |
| `cat <path>` | ログを読む (`/labterm/*.csv` のみ) |
| `shot` | 画面キャプチャ (`uv run tools/shot.py`) |
| `btn a` / `hold b` / `tap x y` / `press x y` | 操作のテスト用 |
| `card on` / `card off` / `card next` | QRカードのテスト用 |

## フォント

`fonts/*.vlw` は `tools/fonts.sh` で TTF から作ってファームに埋め込んでいる。
ソースに新しい日本語の文字列を足したら作り直す。

- Roboto (Apache-2.0)
- Noto Sans CJK JP / Noto Serif CJK JP (SIL OFL-1.1)
- JetBrainsMono Nerd Font のアイコン (SIL OFL-1.1)

## セキュリティ

- `data/config.json` は wifi のパスワードが入るので git に入れない (.gitignore 済み)
- 端末側では平文で保存される。端末/SDカードを持ち出されたら読めるので、研究室の共用wifiのパスワードを入れる場合は注意
- シリアルの `cat` は csv しか読めないようにしてある (config.json は読めない)
- wifi は NTP の時だけつないですぐ切る。サーバ等は立てていない

## TODO

- Google カレンダーの iCal URL から予定を取る (`scheduleSetExternal()` に入れれば表示される)
- Slack / Discord に入退室を通知
- CO2 センサー (Unit CO2) をつないで換気アラート
- 複数人の在室ボード (RFID Unit でカードをかざす)
