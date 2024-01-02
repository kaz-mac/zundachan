/*
  ChatGPT.h
  ズンダチャン ChatGPT CLASS

  Copyright (c) 2024 Kaz  (https://akibabara.com/blog/)
  Released under the MIT license.
  see https://opensource.org/licenses/MIT
*/
#pragma once
#include <M5Unified.h>
#include <HTTPClient.h>
#include <WiFiClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include <deque>  // キューの処理
#include "CharacterConfig.h"  // キャラクター設定

// デバッグに便利なマクロ定義 --------
#define sp(x) Serial.println(x)
#define spn(x) Serial.print(x)
#define spf(fmt, ...) Serial.printf(fmt, __VA_ARGS__)

namespace chat_gpt {

struct HtmlStatus {
  String html;
  uint16_t size;
  uint16_t code;
};
struct ChatHistoryForm {
  int userid;
  String text;
};

class ChatGPT {
public:
  // 可能ならPSRAM上にJSON解析用のメモリを割り当てる
  struct SpiramAllocator {
    void* allocate(size_t size) {
      if (usePsram && heap_caps_get_free_size(MALLOC_CAP_SPIRAM) > size) {
        return ps_malloc(size);
      } else {
        return malloc(size);
      }
    }
    void deallocate(void* pointer) {
      free(pointer);
    }
  };
  using SpiRamJsonDocument = BasicJsonDocument<SpiramAllocator>;
  SpiRamJsonDocument* json; // JSON解析用のオブジェクト SPRAM上に確保されている

  enum HttpMethod { GET, POST, HEAD };

  int preallocateJsonSize = 30*1024;    // JSON解析で使うメモリサイズ
  int preallocatePostSize = 100*1024;   // REST-APIでPOSTするJSONのメモリサイズ（参考:xx文字でxxKBほど使う）
  byte *jsonBuffer;   // AJSON解析で使うメモリのポインタ
  char *postBuffer;   // POSTするJSONデータのメモリのポインタ
  uint16_t maxChatHistory = 20;   // 記憶する会話履歴の最大数

  // メインと同じ変数名で使用する変数
  int characterMaxNum;
  int characterNo;
  char** characterNames;
  char** hostNames;

  WiFiClient client;
  WiFiClientSecure sclient;
  //HTTPClient http;

  bool debug = true;        // シリアルコンソールにデバッグ情報を出力する
  static bool usePsram;    // メモリをPSRAMに確保する
  const char* rootCACertificate = NULL; // ルート証明書
  bool useRootCACertificate = false;    // ルート証明書を使う
  String _apikey = "";   //ChatGPTで使用するAPI KEY
  std::deque<ChatHistoryForm> chatHistory;   // 会話履歴のキュー

  // APIのエンドポイント・デフォルト値
  String endpointCgatGPT     = "https://api.openai.com/v1";

  // キャラ設定
  const String initSystemMessage = CHARACTER_INFO COMMON_INFO;

  ChatGPT();
  //~ChatGPT() = default;
  ~ChatGPT();

  // メンバ関数
  void init(String apikey);    // 初期化、バッファーメモリ確保、
  void setMainValues(int charmax, int charno, char** charnames, char** hostnames);  // キャラクター情報を本クラスに与える
  void setRootCA(const char* root_ca);  // ルート証明書をセットする
  void unsetRootCA();       // ルート証明書を無効にする
  void usePSRAM(bool psram);        // PSRAMを使う
  HtmlStatus httpGetJson(String url, HttpMethod method, bool decodeJson=false, String postData="");  // Webサーバーにアクセスして、JSONをデコードする
  String requestChat(bool addhist);       // ChatGTPにリクエストを送信する
  int addHistory(int userid, String text);  // 会話履歴を追加する

  // その他
  static String URLEncode(const char* msg);
  void debugUrlPrint(String title, String url, int16_t code, String memo="", String html="");  // シリアルコンソールにデバッグ情報を出力する
  void debugJsonPrint(String title, bool pretty=false);    // シリアルコンソールにjsonの内容を出力する

}; //class

// from M5Stack-Avatar (https://github.com/meganetaaan/m5stack-avatar)
// class DriveContextGPT {
//  private:
//   ChatGPT *cgpt;
//  public:
//   DriveContextGPT() = delete;
//   explicit DriveContextGPT(ChatGPT *cgpt);
//   ~DriveContextGPT() = default;
//   DriveContextGPT(const DriveContextGPT &other) = delete;
//   DriveContextGPT &operator=(const DriveContextGPT &other) = delete;
//   DriveContextGPT *getChatGPT();
// };

} //namespace
