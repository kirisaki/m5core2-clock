# M5Core2 Clock

M5Stack Core2 向けの Wi-Fi 時計。Arduino + M5Unified を PlatformIO でビルドします。

## 機能

- RTC からの起動時刻復元と NTP 同期、日本時間での時計表示
- 別 FreeRTOS タスクでの Wi-Fi 接続・再接続
- LAN の温湿度 API から30秒ごとに温度・湿度を取得（`.local` は mDNS で解決）
- Open-Meteo から15分ごとに現在と48時間分の時間別予報を取得
- メイン画面は室内温湿度・現在の天気と気温・次の時間帯の天気と予想気温を横並びに表示
- 天気部分をタッチして現在の時間帯から24時間分の予報へ移動、左上の Back で戻る
- 詳細画面は時刻・天気アイコン・予想気温を6列×2段に配置し、12時間ずつ2ページで表示
- Prev 12h / Next 12h または左右・上下スワイプでページ移動
- 通信エラー時は前回のデータを保持し、古いデータを `OLD` と表示

未取得の値は `--` と表示します。Wi-Fi 未設定でも画面操作を確認できます。
初期 UI のラベルは英語です。
画面下の `S` は温湿度、`W` は天気の取得時刻で、`!` は直近の取得失敗です。
天気はアイコンのみで表し、晴れは昼に太陽、夜に月を表示します。
`ROOM` は室内センサーの実測値、天気欄の気温は Open-Meteo の値です。
メイン画面の気温は小数1桁、12時間予報の気温は整数に丸め、摂氏（`C`）で表示します。

## 環境

PlatformIO Core 6.x を使用します。プラットフォームとライブラリは
`platformio.ini` で固定しています。

| 依存 | バージョン |
| --- | --- |
| PlatformIO Espressif32 | 7.1.3 |
| Arduino-ESP32（上記プラットフォームが提供） | 2.0.17 |
| M5Unified | 0.2.25 |
| M5GFX | 0.2.32 |
| ArduinoJson | 7.4.2 |

PlatformIO Core、Espressif32 プラットフォーム、Arduino-ESP32 は別々の
バージョンです。公式 Espressif32 7.1 系の Arduino は 2.0.17 です。

uv で PlatformIO を管理する場合は、ビルド・書き込みに使う esptool の Python
依存も同じ環境に入れます。

```sh
uv tool install --force --with 'esptool==4.11.0' 'platformio==6.2.0'
pio --version
```

`ModuleNotFoundError: No module named 'intelhex'` が出た場合も上記で修復できます。
`intelhex` 単体でなく、使用する esptool と同じバージョンの依存一式を追加します。

## ビルドと書き込み

```sh
cp include/secrets.example.h include/secrets.h
```

既に `include/secrets.h` がある場合はコピーせず、そのファイルを編集してください。
`include/secrets.h` に 2.4 GHz Wi-Fi の SSID、パスワード、温湿度 API の URL を設定します。
このファイルは Git 管理対象から除外しています。認証情報はファームウェアにも
含まれるため、生成したバイナリの共有時にも注意してください。
設定ファイルがなくてもビルドできます。

| 設定名 | 内容 |
| --- | --- |
| `CLOCK_WIFI_SSID` | Wi-Fi の SSID |
| `CLOCK_WIFI_PASSWORD` | Wi-Fi のパスワード |
| `CLOCK_SENSOR_ENDPOINT` | 温湿度 API の完全な URL（スキーム・パスを含む） |
| `CLOCK_WEATHER_LATITUDE` | 天気の対象地点の緯度（-90〜90） |
| `CLOCK_WEATHER_LONGITUDE` | 天気の対象地点の経度（-180〜180） |

同名の環境変数を渡すと、そのビルドだけローカル設定を上書きします。
設定値は `.pio/build/<環境>/generated/clock_build_config.h` に生成され、
コンパイル時に組み込まれます。コンパイラの `-D` 引数には値を出力しません。
環境変数を外すと次のビルドでローカル設定へ戻ります。
文字列設定は空文字を明示した場合も上書きされます（SSID を空にすると Wi-Fi を無効化）。
緯度・経度は数値で指定し、ローカル設定との合成後に両方が揃う必要があります。
範囲外の値、空文字、NaN、無限大はエラーにします。

```sh
env CLOCK_SENSOR_ENDPOINT='http://sensor.local/api/v1/environment' pio run
env CLOCK_WEATHER_LATITUDE=35.0 CLOCK_WEATHER_LONGITUDE=140.0 pio run
```

優先順は **環境変数 → `include/secrets.h` → 既定値** です。
文字列の既定値は空文字、緯度・経度の両方が未指定なら天気の地点は未設定です。

```sh
pio run
pio device list
pio run -t upload --upload-port /dev/ttyUSB0
pio device monitor --port /dev/ttyUSB0
```

ビルド設定と API 解析・時刻選択は `python3 -m unittest discover -s tests` で
確認できます（ホスト側の `g++` と、`pio pkg install` で取得した ArduinoJson を使用）。

ポート名は `pio device list` の結果に合わせてください。初回ビルドでは依存の
ダウンロードにインターネット接続が必要です。

## 時刻と通信

RTC とシステム時刻は UTC として扱い、表示時に `JST-9` へ変換します。
以前のファームウェアが RTC に日本時間を書いていた場合は、初回 NTP 同期まで
時刻がずれることがあります。RTC の低電圧フラグや不正な日付を検出した場合は
NTP 同期を待ちます。同期後は UTC を RTC に書き戻します。

Wi-Fi 接続と HTTP リクエストは `src/network.cpp` の通信タスクで実行し、
取得結果を長さ1の FreeRTOS キューで UI に渡します。
画面、タッチ、RTC の操作はメインタスクにまとめています。
HTTP の接続・読み取りは各5秒、TLS ハンドシェイクは8秒、応答は最大16 KiBです。
天気の取得失敗時は60秒後に再試行します。

温湿度 API は次の JSON を返すことを想定しています。

```json
{"temperature_c": 25.0, "humidity_percent": 50.0, "sensor": "SHT40", "age_ms": 400}
```

`age_ms` はセンサーの最終測定からの経過ミリ秒です。取得後の経過時間と合わせて
2分以上なら温湿度に `OLD` を表示します。天気は取得から30分以上、または直近の
取得失敗で `OLD` を表示します。JSON 不正時は最後の成功データを維持します。
キャッシュは RAM 上のみで、電源を切ると消えます。

## 天気データ

[Open-Meteo](https://open-meteo.com/) の非商用無料 API を使用します。
API キーは不要です。現在の天気は気象モデルに基づく値です。
日本時間の当日・翌日を取得し、詳細画面では現在の時間帯から24時間分を選択します。
例えば23:30なら、1ページ目は23:00〜翌10:00、2ページ目は翌11:00〜22:00です。
内部では Unix 時刻を用い、日付をまたいでも連続した予報を表示します。
太陽・月の切り替えには API の各時間帯の `is_day` を使用します。

HTTPS のサーバー名・証明書を検証します。`include/weather_ca.h` に ISRG Root X1
を同梱しており、RTC または NTP で時刻が得られるまで天気への接続は待ちます。
API 側の証明書チェーンが変わった場合は信頼する CA の更新が必要です。

Weather data by [Open-Meteo.com](https://open-meteo.com/),
licensed under [CC BY 4.0](https://creativecommons.org/licenses/by/4.0/).
画面にも提供元とライセンスを表示しています。
無料枠・商用利用の条件は [公式料金ページ](https://open-meteo.com/en/pricing) を参照してください。

## 実機確認

- Wi-Fi 未設定でも画面が表示され、天気画面と時計画面を往復できること
- Wi-Fi 設定後に NTP 同期し、日本時間が表示されること
- 同期後、ネットワークがない状態で再起動しても RTC から時刻を復元すること
- 接続先を止めても時計とタッチ操作が続き、復帰後に再接続すること
- 温湿度と天気が取得でき、更新時刻が表示されること
- メイン画面で室内温湿度と天気・気温が横並びに収まり、天気欄のタッチで詳細画面に移ること
- 詳細画面に12時間分の時刻・アイコン・予想気温が収まり、次ページに続きの12時間を表示すること
- ボタン・スワイプでページを移動でき、日付をまたぐ予報も表示できること
- API に接続できなくなった場合に前回値が残り、`OLD` / `!` が表示されること

## 参照

- [Core2 公式ドキュメント](https://docs.m5stack.com/en/core/Core2)
- [PlatformIO Espressif32 リリース](https://github.com/platformio/platform-espressif32/releases)
- [M5Unified](https://github.com/m5stack/M5Unified)
- [M5GFX](https://github.com/m5stack/M5GFX)
