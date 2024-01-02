/*
  ChatGPT.cpp
  ズンダチャン ChatGPT CLASS

  Copyright (c) 2024 Kaz  (https://akibabara.com/blog/)
  Released under the MIT license.
  see https://opensource.org/licenses/MIT
*/
#include "ChatGPT.h"

namespace chat_gpt {

// DriveContextGPT::DriveContextGPT(ChatGPT *cgpt) : cgpt{cgpt} {}
// ChatGPT *DriveContextGPT::getChatGPT() { return cgpt; }

bool ChatGPT::usePsram = true;  // PSRAMを使う（非搭載ならtrueでも使わない）

// コンストラクタ
ChatGPT::ChatGPT() {
  usePsram = false;
  json = nullptr;
}

// デストラクタ
ChatGPT::~ChatGPT() {
  if (json) delete json;
  if (postBuffer) delete postBuffer;
}

// 初期化・出力先の設定とメモリ確保、ChatGPTのAPIキーを設定する
void ChatGPT::init(String apikey) {
  _apikey = apikey;
sp("ChatGPT init");

  // PSRAMが無い場合はPSRAMを使わない
  if (usePsram && heap_caps_get_free_size(MALLOC_CAP_SPIRAM) == 0) {
    usePsram = false;
  }

  // // JSON解析用のメモリを確保する（https://arduinojson.org/v6/assistant/ で計算できる）
  json = new SpiRamJsonDocument(preallocateJsonSize);

  // REST-APIでPOSTするJSONのメモリを確保する
  if (usePsram) {
    postBuffer = (char *)ps_malloc(preallocatePostSize+1);  // PSRAMに確保
  } else {
    postBuffer = (char *)malloc(preallocatePostSize+1);   // SRAMに確保
  }
  if (!postBuffer) {
    Serial.printf("FATAL ERROR:  Unable to preallocate %d bytes for app\n", preallocatePostSize);
  }
  postBuffer[0] = 0;
}

// キャラクター情報を本クラスに与える
void ChatGPT::setMainValues(int charmax, int charno, char** charnames, char** hostnames) {
  characterMaxNum = charmax;
  characterNo = charno;
  characterNames = charnames;
  hostNames = hostnames;
}

// ルート証明書をセットする
void ChatGPT::setRootCA(const char* root_ca) {
  rootCACertificate = root_ca;
  useRootCACertificate = true;
}

// ルート証明書を無効にする
void ChatGPT::unsetRootCA() {
  rootCACertificate = NULL;
  useRootCACertificate = false;
}

// PSRAMを使う
void ChatGPT::usePSRAM(bool psram) {
  usePsram = psram;
}

// APIにアクセスして、JSONをデコードする
HtmlStatus ChatGPT::httpGetJson(String url, HttpMethod method, bool decodeJson, String postData) {
  HTTPClient http;
  HtmlStatus hres = { "", 0, -1 };
  if (_apikey == "") return hres;

  // 接続設定
  http.setTimeout(65000);
  if (url.startsWith("https://")) {
    if (useRootCACertificate) {
      sclient.setCACert(rootCACertificate); // 証明書を設定する
    } else {
      sclient.setInsecure();  // 証明書を検証しない
    }
    http.begin(sclient, url);      
  } else {
    http.begin(client, url);  // 普通のhttp接続
  }
  //http.setReuse(true);
  http.setFollowRedirects(HTTPC_FORCE_FOLLOW_REDIRECTS);

  // ヘッダーの設定
  http.addHeader("Authorization", String("Bearer ") + _apikey);
  http.addHeader("Content-Type", "application/json");

  // データ取得開始
  if (method == HttpMethod::GET) {
    hres.code = http.GET();
  } else if (method == HttpMethod::POST) {
    hres.code = http.POST(postData);
  } else if (method == HttpMethod::HEAD) {
    hres.code = http.sendRequest("HEAD");
  }
  if (hres.code == HTTP_CODE_OK) {
    hres.size = http.getSize();
    String html = http.getString();
    if (hres.size >= 0 && http.connected() && method != HttpMethod::HEAD) {
      // JSONをパースする
      if (decodeJson) {
        DeserializationError error = deserializeJson(*json, html);
        if (error) Serial.println(error.f_str());
      } else {
        hres.html = http.getString();
      }
    }
  }
  http.end();
  return hres;
}

// ChatGTPにリクエストを送信する
String ChatGPT::requestChat(bool addhist) {
  /*
   * 
   * Document: https://platform.openai.com/docs/api-reference
  */
  String url, postData;
  HtmlStatus hres;
  unsigned long timeout = 0;

  // 問い合わせ用のパラメーターを作成する
  json->clear();
  (*json)["model"] = "gpt-3.5-turbo";
  JsonArray jsonMessages = json->createNestedArray("messages");
  for (int i=-1; i<(int)chatHistory.size(); i++) {
    JsonObject jsonMessage = jsonMessages.createNestedObject();
    if (i == -1) {
      if (initSystemMessage == "") continue;
      jsonMessage["role"] = "system";
      jsonMessage["content"] = initSystemMessage; // キャラ設定の文章
    } else {
      if (chatHistory[i].userid == -1) {
        jsonMessage["role"] = "system";
      } else if (chatHistory[i].userid == characterNo) {
        jsonMessage["role"] = "assistant";
      } else {
        jsonMessage["role"] = "user";
        //String str = String(characterNames[chatHistory[i].userid]) + "「" + chatHistory[i].text + "」";
      }
      jsonMessage["content"] = chatHistory[i].text;
    }
  }

  // ChatGPT APIに送信する
  url = endpointCgatGPT + "/chat/completions";
  serializeJson(*json, postData);
  //debugJsonPrint("Post Data", true);  // デバッグ情報
  if (debug) Serial.println("Post Data: "+postData);
  json->clear();
  hres = httpGetJson(url, HttpMethod::POST, true, postData);
  debugUrlPrint("ChatGPT API Request", "POST "+url, hres.code, "", hres.html);  // デバッグ情報
  debugJsonPrint("Response Data", true);  // デバッグ情報

  // レスポンス処理
  String content = "";
  int choiNum = 0;
  if (hres.code == HTTP_CODE_OK) {
    if ((*json).containsKey("choices")) {
      choiNum = (*json)["choices"].size();
      if (choiNum >= 1) {
        content = (*json)["choices"][0]["message"]["content"].as<const char*>();
        addHistory(characterNo, content);
      }
    }
  }
  return content;
}

// 会話履歴を追加する
int ChatGPT::addHistory(int userid, String text) {
  ChatHistoryForm chat = { userid, text };  // キューの最後に追加
  chatHistory.push_back(chat);
  if (chatHistory.size() >= maxChatHistory) {
    chatHistory.pop_front();   // 先頭のキューを削除する
  }
  return chatHistory.size();
}

// URLエンコードを行う
String ChatGPT::URLEncode(const char* msg) {
  const char *hex = "0123456789ABCDEF";
  String encodedMsg = "";

  while (*msg != '\0') {
    if ( ('a' <= *msg && *msg <= 'z')
         || ('A' <= *msg && *msg <= 'Z')
         || ('0' <= *msg && *msg <= '9')
         || *msg  == '-' || *msg == '_' || *msg == '.' || *msg == '~' ) {
      encodedMsg += *msg;
    } else {
      encodedMsg += '%';
      encodedMsg += hex[*msg >> 4];
      encodedMsg += hex[*msg & 0xf];
    }
    msg++;
  }
  return encodedMsg;
}

// シリアルコンソールにデバッグ情報を出力する
void ChatGPT::debugUrlPrint(String title, String url, int16_t code, String memo, String html) {
  if (debug) {
    if (title != "") Serial.printf("## %s ----------\n", title.c_str());
    Serial.printf("VOICEVOX code=%d url=%s\n", code, url.c_str());
    if (memo != "") Serial.printf("memo: %s\n", memo.c_str());
    if (html != "") Serial.printf("html: %s\n", html.c_str());
    Serial.println("");
  }
}

// シリアルコンソールにjsonの内容を出力する
void ChatGPT::debugJsonPrint(String title, bool pretty) {
  if (debug) {
    String debugResponse;
    if (pretty) serializeJsonPretty(*json, debugResponse);
    else serializeJson(*json, debugResponse);
    Serial.println(title+": "+debugResponse);
  }
}

} //namespace
