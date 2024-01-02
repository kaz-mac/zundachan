# ズンダチャン（製作途中ver）
製作途中のプログラムです。いつまでたっても完成しないのでとりあえずアップロードだけしておきます。このままでは多少手を加えないと動きませんが、ご自身の作品で活用していただいて結構です。

ズンダチャンの制作にあたり、robo8080さんの[AIｽﾀｯｸﾁｬﾝ](https://github.com/robo8080/M5Unified_StackChan_ChatGPT)を参考にさせていただきました。

# 概要
これはスタックチャンを元にしたAIロボット「ズンダチャン」のプログラムです。ChatGPT APIを使って会話文を生成し、VOICEVOXを使って音声合成をします。2台のズンダチャンにそれぞれ別々のキャラクターを設定して、AIロボット同士で会話します。

デフォルトではシングルモード（1人だけとの会話）になっています。ずんだもんに話しかけたい場合は、Webブラウザ上からメッセージを送信します。

# 動作環境
* ハードウェア　M5Stack Core2（Core2専用）
* 開発環境　Arduino IDE
* 音声合成　VOICEVOX（WEB版、REST API版で動作。REST API推薦）
* AI環境　ChatGPT API
* その他必要な環境　Python（画像を変換するのに使用）

# ディレクトリ構成
* /tools ... PSD形式の画像からRGB565形式に一括変換するプログラム
* /src ... M5Stackのプログラム
* /docs ... ドキュメント（未作成）

# 製作途中verでも作ってみたいという場合は...
[docs/](docs/) をご覧ください。デバッグ用のシリアル出力や無駄なコメント等が大量にありますが、現バージョンは未完成でまだ製作途中のものです。

# ライセンス
個別に表記しているものを除き、[MIT license](https://opensource.org/licenses/MIT)です。<br>
AudioFileSourceHTTPStream2.* はGPLです。