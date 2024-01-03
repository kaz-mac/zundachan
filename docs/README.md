# ドキュメント制作中
ドキュメントは制作中です。

# ファイルの説明
| ファイル名  | 用途 |
| ------------- | ------------- |
| ApiKey.h | WiFiの接続情報やAPI KEYを記述するファイル |
| CharacterConfig.h | キャラクターの設定を記述するファイル |
| image_zundamon.h | ずんだもんの画像データ |
| sound.h | 音声データ（起動時/応答時用） |
| zundachan.ino | メインプログラム |

# クラスの概要
| クラス名  | 用途 |
| ------------- | ------------- |
| ChatGPT | ChatGPTとのデータのやり取りを行う |
| ServoChan | サーボモーターの制御 |
| VoicevoxTTS | VOICEVOXによる音声合成の処理 |
| WebInterface | 内蔵Webサーバーおよび、ズンダチャン間のAPI通信の処理 |
| Zundavatar | アバターの描画 |

# 大雑把なセットアップ手順
まともなドキュメントは無くてもヒントさえあれば大丈夫だよ！という方向けに、とても大雑把なセットアップ手順を書いておきます。

## おおまかな流れ
1. ずんだもんの立ち絵素材を変換する
2. 起動サウンドを作成する
3. VOICEVOXの準備をする
4. Aruduino IDEでコンパイルする
5. ズンダチャンを起動する

## STEP 1 : ずんだもんの立ち絵素材を変換する
詳しくは tools/ の[ドキュメント](../tools/) をご覧ください。ずんだもんと四国めたんの設定ファイル(.ini)とバッチファイル(.bat)がサンプルで入ってますので、それを使えばとりあえず変換だけはできます。保存するファイル名は image_zundamon.h です。変換したファイルはそのままコピーすれば使えます。

立ち絵素材は坂本アヒルさんの[ずんだもんの立ち絵素材](https://www.pixiv.net/artworks/92641351)の使用を想定しています。

## STEP 2 : 起動サウンドを作成する
起動時や体に触れたときに喋る用のサウンドファイルを作成します。まずはVOICEVOXで好きな言葉を喋らせて.wav形式で保存します。次にWAVファイルをソースコード形式に変換します。詳しくは tools/ の[ドキュメント](../tools/) をご覧ください。保存するファイル名は sound.h です。変換されたデータ部分をコピー＆ペーストしてください。

## STEP 3 : VOICEVOXの準備をする
1. [VOICEVOX](https://voicevox.hiroshiba.jp/)をインストールします。
2. PCのIPアドレスを調べて控えておきます。ズンダチャンからLAN経由でPCにアクセスしますので、必要に応じてファイアウォールの設定してください。VOICEVOXは普通に起動するとlocalhostからしかアクセスできません。そこで、以下のようなバッチファイルを作っておくと、プライベートIPでアクセスでアクセスできて便利です。
`C:\Users\xxx\AppData\Local\Programs\VOICEVOX/run.exe --host 192.168.xx.xxx --port 50021`
3. ブラウザで開いてアクセスできるか確認します。 http://192.168.xx.xxx:50021/docs/ APIの説明ページが表示されればOKです。別の端末からもアクセスできるか確認しておきましょう。

## STEP 4 : Aruduino IDEでコンパイルする
1. STEP 1, 2で作成した .h ファイルをsrcに入っているファイルと同じディレクトリにコピーします。
2. ApiKey.h を自分の環境に合わせて変更します。WiFiのSSID/PASSとChatGPT API KEYは必須です。VOICEVOX WEB版のAPI KEYは使用しなければ空欄でかまいません。デフォルトではREST APIを使用します。VOICEVOXのREST APIというのは、ローカルのPCにインストールしたVOICEVOXを使用し、PCのGPUで音声合成を行う方法です。
WEB版の方を使いたい場合は、メインプログラムのtts.initのところを修正すればできます。コメントを参考にしてください。なお、WEB版のリップシンクは未制作です。
3. メインプログラムzundachan.inoの VOICEVOX_RESTAPI_ENDPOINT に書かれているIPアドレスを、VOICEVOXが稼働しているPCのIPアドレスに書き換えます。
4. キャラクター設定を変更したい場合は CharacterConfig.h を編集してください。デフォルトではずんだもんが四国めたんと喋ってるという想定で作っています。
5. Arduino IDEで必要なライブラリをインポートしてください。何が必要かは、各ソースプログラムの #include 行を参考にしてください。（おそらくM5Unified、ESP8266Audio、HTTPClient、ArduinoJson、ServoEasing、ESP32Servoくらいでいいかと）Arduino IDEのライブラリマネージャーからインストールできないライブラリ(ESP32WebServer)については、記載の[URL](https://github.com/Pedroalbuquerque/ESP32WebServer)からダウンロードできると思います。
6. それではいよいよコンパイルです。M5Stack Core 2を接続します。PSRAMがenableになっているか確認してください。

## STEP 5 : ズンダチャンを起動する
1. VOICEVOXを起動していない場合は、STEP 3を参考に起動します。
2. ズンダチャンを起動するとArduino IDEのシリアルモニターにいろいろ表示されます。WiFi接続後に以下のように表示されることを確認してください。
`mDNS registered. http://zunda.local/`
3. スマホやPCなどから http://zunda.local/ にアクセスします。
4. メッセージ欄に何か入力して話しかけてみましょう。四国めたんと会話している想定になっているので、ずんだもんは四国めたんだと思って返事をすると思います。

# AIロボット同士で会話させるには？
デフォルトでは1人 vs スマホでの会話モードになっていますが、2人のAIロボット同士で会話させることもできます。
1. 2台のM5Stack Core2 を用意する。
2. 四国めたんの画像を変換する。（サンプルで入ってるバッチファイルで一括変換できると思います）
3. zundachan.ino の singleMode=false; にする。
4. CharacterConfig.h の CHARACTER_NO を0（ずんだもん）と1（四国めたん）にして、それぞれコンパイルして書き込む。
5. 起動したらスマホやPCなどから http://zunda.local/ にアクセスして、第三者として話題を提供する。 http://metan.local/ にアクセスすることも可能。

これでAI同士が勝手に喋ってくれると思います。
