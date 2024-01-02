/*
  WebInterface.h
  ズンダチャン Webサーバー CLASS

  Copyright (c) 2023 kaz  (https://akibabara.com/blog/)
  Released under the MIT license.
  see https://opensource.org/licenses/MIT
*/
#pragma once

#include <ESP32WebServer.h>   // https://github.com/Pedroalbuquerque/ESP32WebServer
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFiClient.h>

//#include "CharacterConfig.h"
#include "VoicevoxTTS.h"
#include "ChatGPT.h"

// デバッグに便利なマクロ定義 --------
#define sp(x) Serial.println(x)
#define spn(x) Serial.print(x)
#define spf(fmt, ...) Serial.printf(fmt, __VA_ARGS__)

namespace web_interface {

struct Notice {   // 外部からのリクエストを保存しておく変数用
  bool newMessage = false;      // 他のズンダチャンからの新着メッセージの有無（textは自動的に履歴に入る）
  int pressButton = 0;          // Webで押したボタン
  int changeVolume = -1;        // Webで変更した音量
  bool newExmessage = false;    // Webから送信されたメッセージの有無
  String newExmessageText = ""; // Webから送信されたメッセージのtext
  int singleModeChange = -1;    // シングルモードの変更
};

struct FriendStatus {
  bool success = false;
  int characterNo = 0;
  bool playlable = false;
};

class WebInterface {
public:
  ESP32WebServer server;
  WiFiClient client;
  TaskHandle_t xLoopHandle = nullptr;
  voicevox_tts::VoicevoxTTS* ttsPtr = nullptr;
  chat_gpt::ChatGPT* gptPtr = nullptr;
  Notice notice;

  // メインと同じ変数名で使用する変数
  int characterMaxNum;
  int characterNo;
  char** characterNames;
  char** hostNames;

  WebInterface() : server(80) {};
  ~WebInterface();

  // メンバ関数
  void setMainValues(int charmax, int charno, char** charnames, char** hostnames);  // キャラクター情報を本クラスに与える
  void webSetup(voicevox_tts::VoicevoxTTS* p1=nullptr, chat_gpt::ChatGPT* p2=nullptr);    // Webサーバーの初期設定を行う、各クラスのポインタを渡す
  //static void webLoop0(void* _this);     // Webサーバーのループ処理
  void webLoop();     // Webサーバーのループ処理
  String tf(bool b);
  bool sendTalk(String text, int fromCharactorNo, int toCharactorNo, bool noop); // 会話を送信する
  FriendStatus checkFriendStatus(int toCharactorNo);    // 相手の状態を取得する

  // Webサーバーの処理
  void handleRoot();      // トップページ
  void apiStatus();       // API 状態を応答する
  void apiReceiveTalk();  // API 会話を受信する
  void apiButton();       // API ボタン操作
  void apiVolume();       // API ボリューム
  void apiExmessage();    // API 外部からの会話用メッセージ
  void apiSingleMode();   // API シングルモード

}; //class


//const char htmlHeaderText[] PROGMEM = "<html lang=\"ja\"><head><meta charset=\"UTF-8\"><title>Zunda-chan</title></head><body>\n"; 
//const char htmlFooterText[] PROGMEM = "</body></html>\n";

// TOPページのHTML
static const char HTML_CPANEL[] PROGMEM = R"EOT(
<!DOCTYPE html>
<html lang="ja">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<style>
  body { font-family: Arial, sans-serif; padding: 10px; }
  #title { text-align: center; font-size: 24px; margin-bottom: 20px; }
  #buttons { margin-bottom: 20px; text-align: center; }
  .btn {
    padding: 15px 30px; 
    font-size: 18px; 
    margin: 0 10px; 
    vertical-align: top;
  }
  #volumeLabel {
    display: block;
    margin: 0 0 10px 0;
    font-size: 16px;
  }
  #slider { width: 100%; }
  #status {
    margin-top: 20px;
    color: red;
    white-space: pre-wrap;
  }
  #messageForm {
    display: flex;
    margin-top: 20px;
  }
  #messageInput {
    flex: 1;
    margin-right: 10px;
    height: 38px;
    padding: 5px;
  }
  #sendMessage {
    padding: 10px 20px;
    font-size: 18px;
  }
</style>
</head>
<body>

<div id="title">ズンダチャン・コントローラー</div>

<div id="buttons">
  <button class="btn" onclick="sendRequest(1)">サーボ<br />停止</button>
  <button class="btn" onclick="sendRequest(2)">B</button>
  <button class="btn" onclick="sendRequest(3)">再生<br />停止</button>
</div>

<label for="slider" id="volumeLabel">ボリューム</label>
<input type="range" id="slider" min="0" max="255" value="128" onchange="sendSliderValue()">

<label for="slider" id="volumeLabel">メッセージ</label>
<div id="messageForm">
  <textarea id="messageInput" rows="2"></textarea>
  <button id="sendMessage" onclick="sendMessage()">送信</button>
</div>

<label for="buttons" id="volumeLabel">シングルモード</label>
<div>
  <input type="radio" id="off" name="single" value="off" onclick="sendRequest2(0)" checked>
  <label for="off">OFF</label>
  <input type="radio" id="on" name="single" value="on" onclick="sendRequest2(1)">
  <label for="on">ON</label>
</div>

<div id="status"></div>

<script>
function sendRequest(btn) {
  fetch(`/api/button?btn=${btn}`)
    .then(response => response.json())
    .then(data => {
      document.getElementById('status').value = data.message;
    })
    .catch(error => {
      console.error('Error:', error);
    });
}

function sendRequest2(btn) {
  fetch(`/api/singlemode?btn=${btn}`)
    .then(response => response.json())
    .then(data => {
      document.getElementById('status').value = data.message;
    })
    .catch(error => {
      console.error('Error:', error);
    });
}

function sendSliderValue() {
  const sliderValue = document.getElementById('slider').value;
  fetch(`/api/volume?volume=${sliderValue}`)
    .then(response => response.json())
    .then(data => {
      document.getElementById('status').value = data.message;
    })
    .catch(error => {
      console.error('Error:', error);
    });
}

function sendMessage() {
  const message = document.getElementById('messageInput').value;
  fetch("/api/exmessage", {
    method: 'POST',
    headers: {
      'Content-Type': 'application/x-www-form-urlencoded',
    },
    body: `message=${encodeURIComponent(message)}`
  })
  .then(response => response.json())
  .then(data => {
    document.getElementById('status').textContent = data.message;
    document.getElementById('messageInput').value = '';
  })
  .catch(error => {
    console.error('Error:', error);
  });
}
</script>

</body>
</html>
)EOT";

} //namespace
