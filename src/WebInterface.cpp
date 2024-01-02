/*
  WebInterface.cpp
  ズンダチャン Webサーバー CLASS

  Copyright (c) 2024 Kaz  (https://akibabara.com/blog/)
  Released under the MIT license.
  see https://opensource.org/licenses/MIT
*/
#include "WebInterface.h"

namespace web_interface {

// デストラクタ
WebInterface::~WebInterface() {
  if (xLoopHandle != nullptr) {
    vTaskDelete(xLoopHandle);
    xLoopHandle = nullptr;
  }
}

// キャラクター情報を本クラスに与える
void WebInterface::setMainValues(int charmax, int charno, char** charnames, char** hostnames) {
  characterMaxNum = charmax;
  characterNo = charno;
  characterNames = charnames;
  hostNames = hostnames;
}

// Webサーバーの初期設定を行う
void WebInterface::webSetup(voicevox_tts::VoicevoxTTS* p1, chat_gpt::ChatGPT* p2) {
  // クラスのポインターを受け取る
  if (p1 != nullptr) ttsPtr = p1;
  if (p2 != nullptr) gptPtr = p2;

  // Webサーバーの設定
  server.on("/", [this]() { handleRoot(); });
  server.on("/api/status", [this]() { apiStatus(); });
  server.on("/api/receive_talk", [this]() { apiReceiveTalk(); });
  server.on("/api/button", [this]() { apiButton(); });
  server.on("/api/volume", [this]() { apiVolume(); });
  server.on("/api/exmessage", [this]() { apiExmessage(); });
  server.on("/api/singlemode", [this]() { apiSingleMode(); });
  server.on("/inline", [this](){
    server.send(200, "text/plain", "this works as well");
  });
  server.onNotFound([this](){
    server.send(404, "text/plain", "File Not Found.");
  });
  server.begin();

  return;   // 以下、タスクは使ってない 

  // タスクを作成する
  /*
  xTaskCreateUniversal(
    webLoop0,  // Function to implement the task
    "webLoop0",// Name of the task
    2048,           // Stack size in words
    this,            // Task input parameter
    1,             // Priority of the task
    &xLoopHandle,           // Task handle.
    CONFIG_ARDUINO_RUNNING_CORE);
  */
}

// Webサーバーのループ処理（タスクver。Stack Overflowで落ちるので使ってない）
/*
void WebInterface::webLoop0(void* _this) {
  WebInterface* webInterface  = static_cast<WebInterface*>(_this);
  for (;;) {
    webInterface ->server.handleClient();
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}
*/

// Webサーバーのループ処理
void WebInterface::webLoop() {
  server.handleClient();
}

// Webサーバー：PATH=/
void WebInterface::handleRoot() {
  // String html = String(FPSTR(htmlHeaderText));
  // html += "<h2>Zunda-chan Status</h2>\n";
  // html += "Character No: " + String(characterNo) + "<br>\n";
  // html += "Character Name: " + String(characterNames[characterNo]) + "<br>\n";
  // html += "Playable: " + tf(ttsPtr != nullptr && ttsPtr->isNowPlayable()) + "<br>\n";
  // html += String(FPSTR(htmlFooterText));
  server.send(200, "text/html", HTML_CPANEL);
}

// 自分の状態を応答する
void WebInterface::apiStatus() {
  DynamicJsonDocument json(128);
  String responseData;
  json["success"] = 1;
  json["char"] = characterNo;
  json["play"] = ttsPtr->isNowPlayable();
  serializeJson(json, responseData);
  server.send(200, "application/json", responseData);
}

// 相手の状態を取得する
FriendStatus WebInterface::checkFriendStatus(int toCharactorNo) {
  HTTPClient http;
  DynamicJsonDocument json(128);
  FriendStatus stat;

  // http接続
  String url = "http://" + String(hostNames[toCharactorNo]) + ".local/api/status";
  http.setTimeout(5000);
  http.begin(client, url);
  auto code = http.GET();

  // レスポンスの処理
sp("checkFriendStatus code="+String(code));
  if (code == HTTP_CODE_OK) {
    DeserializationError error = deserializeJson(json, http.getString());
    if (!error) {
      if (json.containsKey("success") && json["success"].as<int>() == 1) {
        stat.success = true;
        stat.characterNo = json["char"].as<int>();
        stat.playlable = json["play"].as<bool>();
      }
    }
  }
  return stat;
}

// 会話を送信する
bool WebInterface::sendTalk(String text, int fromCharactorNo, int toCharactorNo, bool noop) {
  HTTPClient http;
  DynamicJsonDocument json(512);
  String postData;
  bool success = false;

  // 送信データの作成
  json["from"] = fromCharactorNo;
  json["to"] = toCharactorNo;
  json["text"] = text;
  json["noop"] = noop;
  serializeJson(json, postData);

  // http接続して送信する
  String url = "http://" + String(hostNames[toCharactorNo]) + ".local/api/receive_talk";
  http.setTimeout(5000);
  http.begin(client, url);
  http.addHeader("Content-Type", "application/json");
sp(url);
sp(postData);
  int code = http.POST(postData);
  if (code == HTTP_CODE_OK) success = true;

  return success;
}

// API: 会話を受信する PATH=/api/receive_talk
void WebInterface::apiReceiveTalk() {
  DynamicJsonDocument json(512);
  String html = "", message = "";
  bool success = false;
  bool noop = false;
  int from;

  if (server.method() == HTTP_POST) {
    html = server.arg(0);
  } else if (server.method() == HTTP_GET) {
    if (server.hasArg("json")) html = server.arg("json");
  }
  DeserializationError error = deserializeJson(json, html);
  if (!error) {
    if (json.containsKey("from") && json.containsKey("text")) {
      from = json["from"].as<int>();
      noop = json["noop"].as<bool>();
sp("noop="+tf(noop)+" from="+String(from));
      if (from >= -1 && from < characterMaxNum) {
sp("from");
        message = String(json["text"].as<const char*>());
        String fromName = (from == -1) ? "システム": String(characterNames[from]);
        Serial.print("Receive From: " + fromName +"\n");
        Serial.print("Message Text: " + message +"\n");
        success = true;
      }
    }
  }

  // レスポンス処理
  if (success) {
    server.send(200, "text/html", "OK");
  } else {
    server.send(400, "text/html", "ERROR");
  }

  // ChatGPTの会話履歴に追加する
  if (success) {
    gptPtr->addHistory(from, message);
    if (! noop) notice.newMessage = true;
  }
}

// API: ボタン操作 PATH=/api/button
void WebInterface::apiButton() {
  int pressButton = server.arg("btn").toInt();
  sp("## button pressed! "+String(pressButton));
  if (pressButton > 0) {
    notice.pressButton = pressButton;
    server.send(200, "text/html", "{\"message\":\"button pressed\"}");
  } else {
    server.send(400, "text/html", "{\"message\":\"button error\"}");
  }
}

// API: ボリューム PATH=/api/volume
void WebInterface::apiVolume() {
  int volume = server.arg("volume").toInt();
  sp("## slider changed! "+String(volume));
  if (volume >= 0 && volume <= 255) {
    notice.changeVolume = volume;
    server.send(200, "text/html", "{\"message\":\"slider changed\"}");
  } else {
    server.send(400, "text/html", "{\"message\":\"slider error\"}");
  }
}

// API 外部からの会話用メッセージ PATH=/api/exmessage
void WebInterface::apiExmessage() {
  if (server.arg("message").length() > 0) {
    sp("## external message received! "+server.arg("message"));
    String msg = "Message from system to "+String(characterNames[characterNo])+": "+server.arg("message");
    notice.newExmessageText = msg;
    gptPtr->addHistory(-1, msg);  // ChatGPTの会話履歴に追加する（characterNo=-1はsystem）
    notice.newExmessage = true;
    server.send(200, "text/html", "{\"message\":\"message accepted\"}");
  } else {
    server.send(400, "text/html", "{\"message\":\"message error\"}");
  }
}

// API: シングルモード PATH=/api/singlemode
void WebInterface::apiSingleMode() {
  int btn = server.arg("btn").toInt();
  sp("## singlemode pressed! "+String(btn));
  notice.singleModeChange = (btn==0 || btn==1) ? btn : -1;
  if (btn != -1) {
    server.send(200, "text/html", "{\"message\":\"singlemode pressed\"}");
  } else {
    server.send(400, "text/html", "{\"message\":\"singlemode error\"}");
  }
}

// デバッグ用
String WebInterface::tf(bool b) {
  return (b) ? "true" : "false";
}

} //namespace
