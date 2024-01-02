/*
  VoicevoxTTS.h
  ズンダチャン VOICEVOX Text-to-Speach CLASS

  Copyright (c) 2024 Kaz  (https://akibabara.com/blog/)
  Released under the MIT license.
  see https://opensource.org/licenses/MIT
*/
#pragma once

#include <M5Unified.h>
#include <HTTPClient.h>
#include <WiFiClient.h>
#include <WiFiClientSecure.h>
//#include <AudioOutput.h>
#include "AudioOutputM5Speaker.h"   // M5UnifiedでESP8266Audioを使うためのクラス
//#include <AudioFileSource.h>
#include <AudioFileSourceBuffer.h>
#include "AudioFileSourceHTTPStream2.h" // AudioFileSourceHTTPStreamのhttps両対応版
#include <AudioFileSourcePROGMEM.h>
#include <AudioGeneratorMP3.h>
#include <AudioGeneratorWAV.h>
#include <ArduinoJson.h>

#define MAX_VOWEL_HISTORY 201   // VOICEVOX REST APIから取得したリップシンク用データの保持数

// デバッグに便利なマクロ定義 --------
#define sp(x) Serial.println(x)
#define spn(x) Serial.print(x)
#define spf(fmt, ...) Serial.printf(fmt, __VA_ARGS__)


namespace voicevox_tts {

enum VoicevoxApiType {  // 使用するAPIの選択
  None,           // 未設定
  WebApiSlow,     // WEB版VOICEVOX API（低速）  https://voicevox.su-shiki.com/su-shikiapis/ttsquest/
  WebApiFast,     // WEB版VOICEVOX API（高速）  https://voicevox.su-shiki.com/su-shikiapis/
  WebApiStream,   // WEB版VOICEVOX API（Stream）https://github.com/ts-klassen/ttsQuestV3Voicevox
  RestApi         // VOICEVOX REST-API  http://localhost:50021/docs
};
enum AudioFormat : uint8_t  { mp3, wav }; // オーディオフォーマット
enum VVVowel : uint8_t { null, a, i, u, e, o, n };  // リップシンク用の母音

struct VowelData {  // VOICEVOX REST APIの時系列母音データ格納用
  VVVowel vowel;
  uint16_t timeline;
};
struct HtmlStatus {
  String html;
  uint16_t size;
  int code;
};

class VoicevoxTTS {
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

  int preallocateBufferSize = 30*1024;  // AudioFileSourceBuffer()で使うメモリサイズ
  int preallocateJsonSize = 30*1024;    // JSON解析で使うメモリサイズ
  int preallocatePostSize = 100*1024;   // REST-APIでPOSTするJSONのメモリサイズ（参考:280文字で20KBほど使う）
  uint8_t *preallocateBuffer; // AudioFileSourceBuffer()で使うメモリのポインタ
  byte *jsonBuffer;   // AJSON解析で使うメモリのポインタ
  char *postBuffer;   // POSTするJSONデータのメモリのポインタ

  AudioOutputM5Speaker *_out;
  AudioGeneratorMP3 *mp3;
  AudioGeneratorWAV *wav;
  AudioFileSourceBuffer *buff = nullptr;
  AudioFileSourceHTTPStream2 *file = nullptr;
  AudioFileSource *filepg = nullptr;
  AudioFormat format;

  WiFiClient client;
  WiFiClientSecure sclient;
  //HTTPClient http;

  bool debug = true;        // シリアルコンソールにデバッグ情報を出力する
  bool nowPlaying = false;  // 音声再生中はtureになる
  bool nowAutoPlaying = false;  // 自動再生タスク実行中はtureになる
  static bool usePsram;    // メモリをPSRAMに確保する
  const char* rootCACertificate = NULL; // ルート証明書
  bool useRootCACertificate = false;    // ルート証明書を使う
  VoicevoxApiType apiType;  // 使用するAPIの種類
  String _apikeyWeb = "";   // WEB版VOICEVOX APIで使用するAPI KEY
  uint8_t characterID = 1;    // キャラクターID（話者id）

  // APIのエンドポイント・デフォルト値
  String endpointWebApiSlow  = "https://api.tts.quest/v3/voicevox/synthesis";
  String endpointWebApiFast  = "https://deprecatedapis.tts.quest/v2/voicevox/audio/";
  String endpointWebApiStrem = "https://api.tts.quest/v3/voicevox/synthesis";
  String endpointKeyPoint    = "https://api.tts.quest/v3/key/points";
  String endpointRestApi     = "http://127.0.0.1:50021";
  unsigned long VoicevoxStatusWaitTime = 500;  // ステータス更新の確認を繰り返す間隔(ms)
  unsigned long VoicevoxGenerateTimeout = 20000; // 音声合成の完了を待つ時間(ms)

  static const int levelsCnt = 8; // 音声レベルの保存数
  int levels[levelsCnt];    // 音声レベル配列
  int levelsIdx = 0;        // 上記インデックス
  VowelData vowelHistories[MAX_VOWEL_HISTORY];  // VOICEVOX REST APIから取得したリップシンク用データ
  VVVowel _nowPlayingVowel = VVVowel::null;   // 現在発話中の母音
  uint16_t _nowPlayingLength = 0;   // 現在発話中の母音の長さ(ms)

  VoicevoxTTS();
  //~VoicevoxTTS() = default;
  ~VoicevoxTTS();

  // メンバ関数
  void init(AudioOutputM5Speaker *out, VoicevoxApiType type=VoicevoxApiType::None);    // 初期化・出力先の設定とバッファーメモリ確保
  void changeCharacter(uint8_t id); // キャラクターID（話者id）を変更する
  void setApikey(String apikeyWeb);  // WEB版VOICEVOXのAPIキーを設定する
  void setRootCA(const char* root_ca);  // ルート証明書をセットする
  void unsetRootCA();       // ルート証明書を無効にする
  void usePSRAM(bool psram);        // PSRAMを使う
  void setEndpoint(VoicevoxApiType apiType, String url); // APIのエンドポイントを設定する
  //HtmlStatus httpRequest(String url, String method="GET");  // httpでGETアクセスを行う
  HtmlStatus httpGetJson(String url, HttpMethod method, bool decodeJson=false, String postData="");  // Webサーバーにアクセスして、JSONをデコードする
  void speak(String text, bool waiting=true);              // テキストを喋る
  void speakWebApiSlow(String text);    // テキストを喋る WEB版VOICEVOX API（低速）
  void speakWebApiFast(String text);    // テキストを喋る WEB版VOICEVOX API（高速）
  void speakWebApiStream(String text);  // テキストを喋る WEB版VOICEVOX API（Stream）
  void speakRestApi(String text);       // テキストを喋る VOICEVOX REST-API
  void playUrl(String url, AudioFormat format, bool post=false, char* data=nullptr); // 指定URLから音声ファイルをダウンロードして再生する
  void playUrlMP3(String url, bool post=false, char* data=nullptr) { playUrl(url, AudioFormat::mp3, post, data); }  // 〃 MP3
  void playUrlWAV(String url, bool post=false, char* data=nullptr) { playUrl(url, AudioFormat::wav, post, data); }  // 〃 WAV
  void playProgmem(const unsigned char* data, size_t size, AudioFormat audioformat);  // PROGMEMの音声ファイル再生する
  void playAudio(AudioFileSourceBuffer *buff);  // 再生開始
  void stopAudio();           // 再生停止（再生の停止とメモリ開放）
  void startAutoPlay();     // 自動音声再生を開始する
  void stopAutoPlay();      // 自動音声再生を終了する
  bool awaitPlayable(unsigned long timeout=60000);  // 再生可能になるまで待つ、タイムアウトあり
  bool isNowPlayable();       // 今再生可能か？
  int getLevel();           // 現在の再生中の音声レベルを求める
  VowelData getVowel();           // 現在の発話中の母音を求める
  long getApiKeyPoint();   // APIの残りポイントを取得する

  // その他
  static void MDCallback(void *cbData, const char *type, bool isUnicode, const char *string);
  static void StatusCallback(void *cbData, int code, const char *string);
  static String URLEncode(const char* msg);
  void debugUrlPrint(String title, String url, int16_t code, String memo="", String html="");  // シリアルコンソールにデバッグ情報を出力する

  // デバッグ。後で消す
  void debug_free_memory(String str);

// private:
//   TaskHandle_t handleAudioOutputLoop;
}; //class

// from M5Stack-Avatar (https://github.com/meganetaaan/m5stack-avatar)
class DriveContextTTS {
 private:
  VoicevoxTTS *vvtts;
 public:
  DriveContextTTS() = delete;
  explicit DriveContextTTS(VoicevoxTTS *vvtts);
  ~DriveContextTTS() = default;
  DriveContextTTS(const DriveContextTTS &other) = delete;
  DriveContextTTS &operator=(const DriveContextTTS &other) = delete;
  VoicevoxTTS *getVoicevoxTTS();
};

} //namespace
