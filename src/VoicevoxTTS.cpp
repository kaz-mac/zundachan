/*
  VoicevoxTTS.cpp
  ズンダチャン VOICEVOX Text-to-Speach CLASS

  Copyright (c) 2024 Kaz  (https://akibabara.com/blog/)
  Released under the MIT license.
  see https://opensource.org/licenses/MIT
*/
// TODO: httpアクセスのプロセスはメインCPUで行えないか？？
#include "VoicevoxTTS.h"

namespace voicevox_tts {

DriveContextTTS::DriveContextTTS(VoicevoxTTS *vvtts) : vvtts{vvtts} {}
VoicevoxTTS *DriveContextTTS::getVoicevoxTTS() { return vvtts; }

bool voicevox_tts::VoicevoxTTS::usePsram = true;  // PSRAMを使う（非搭載ならtrueでも使わない）

// デバッグ。後で消す
// 空きメモリ確認
void VoicevoxTTS::debug_free_memory(String str) {
  sp("## "+str);
  spf("heap_caps_get_free_size(MALLOC_CAP_DMA):%d\n", heap_caps_get_free_size(MALLOC_CAP_DMA) );
  spf("heap_caps_get_largest_free_block(MALLOC_CAP_DMA):%d\n", heap_caps_get_largest_free_block(MALLOC_CAP_DMA) );
  spf("heap_caps_get_free_size(MALLOC_CAP_SPIRAM):%d\n", heap_caps_get_free_size(MALLOC_CAP_SPIRAM) );
  spf("heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM):%d\n\n", heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM) );
}

// コンストラクタ
VoicevoxTTS::VoicevoxTTS() {
  usePsram = false;
  json = nullptr;
  nowPlaying = false;
  nowAutoPlaying = false;
  vowelHistories[0].vowel = VVVowel::null;
  vowelHistories[0].timeline = 0;
}

// デストラクタ
VoicevoxTTS::~VoicevoxTTS() {
  if (json) delete json;
  if (preallocateBuffer) delete preallocateBuffer;
  if (postBuffer) delete postBuffer;
}

// 初期化・出力先の設定とメモリ確保
void VoicevoxTTS::init(AudioOutputM5Speaker *out, VoicevoxApiType type) {
  _out = out;
  apiType = type;
  nowPlaying = false;
  nowAutoPlaying = false;
sp("VoicevoxTTS init");

  // PSRAMが無い場合はPSRAMを使わない
  if (usePsram && heap_caps_get_free_size(MALLOC_CAP_SPIRAM) == 0) {
    usePsram = false;
  }

  // ESP8266AudioのAudioFileSourceBuffer()で使うメモリを確保する
  if (usePsram) {
    preallocateBuffer = (uint8_t *)ps_malloc(preallocateBufferSize);  // PSRAMに確保
  } else {
    preallocateBuffer = (uint8_t *)malloc(preallocateBufferSize);   // SRAMに確保
  }
  if (!preallocateBuffer) {
    Serial.printf("FATAL ERROR:  Unable to preallocate %d bytes for app\n", preallocateBufferSize);
  }

  // // JSON解析用のメモリを確保する（https://arduinojson.org/v6/assistant/ で計算できる）
  json = new SpiRamJsonDocument(preallocateJsonSize);

  // REST-APIでPOSTするJSONのメモリを確保する
  if (type == VoicevoxApiType::RestApi) {
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
}

// キャラクターID（話者id）を変更する
void VoicevoxTTS::changeCharacter(uint8_t id) {
  characterID = id;
}

// WEB版VOICEVOX API（高速）のAPIキーを設定する
void VoicevoxTTS::setApikey(String apikeyWeb) {
  _apikeyWeb = apikeyWeb;
}

// ルート証明書をセットする
void VoicevoxTTS::setRootCA(const char* root_ca) {
  rootCACertificate = root_ca;
  useRootCACertificate = true;
}

// ルート証明書を無効にする
void VoicevoxTTS::unsetRootCA() {
  rootCACertificate = NULL;
  useRootCACertificate = false;
}

// PSRAMを使う
void VoicevoxTTS::usePSRAM(bool psram) {
  usePsram = psram;
}

// APIのエンドポイントを設定する
void VoicevoxTTS::setEndpoint(VoicevoxApiType apiType, String url) {
  if (apiType == VoicevoxApiType::RestApi) {
    endpointRestApi = url;
  }
}

// httpでGETアクセスを行う
// HtmlStatus VoicevoxTTS::httpRequest(String url, String method) {
//   HtmlStatus hres = { "", 0, -1 };
//
//   if (url.startsWith("https://")) {
//     if (useRootCACertificate) {
//       sclient.setCACert(rootCACertificate); // 証明書を設定する
//     } else {
//       sclient.setInsecure();  // 証明書を検証しない
//     }
//     http.begin(sclient, url);      
//   } else {
//     http.begin(client, url);  // 普通のhttp接続
//   }
//   http.setReuse(true);
//   http.setFollowRedirects(HTTPC_FORCE_FOLLOW_REDIRECTS);
//   if (method == "GET") {
//     hres.code = http.GET();
//   } else if (method == "HEAD") {
//     hres.code = http.sendRequest("HEAD");
//   }
//   if (hres.code == HTTP_CODE_OK) {
//     if (method != "HEAD") hres.html = http.getString();
//     hres.size = http.getSize();
//   }
//   http.end();
//
//   return hres;
// }

// Webサーバーにアクセスして、JSONをデコードする
HtmlStatus VoicevoxTTS::httpGetJson(String url, HttpMethod method, bool decodeJson, String postData) {
  HTTPClient http;
  HtmlStatus hres = { "", 0, -1 };

  // 接続
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
    if (hres.size >= 0 && http.connected() && method != HttpMethod::HEAD) {
      // JSONをパースする
      if (decodeJson) {
        Stream* stream = http.getStreamPtr();
        DeserializationError error = deserializeJson(*json, *stream);
        if (error) Serial.print(error.f_str());
      } else {
        //hres.html = http.getString();
      }
    }
  }
  http.end();
  return hres;
}

// テキストを喋る
void VoicevoxTTS::speak(String text, bool waiting) {
  if (debug) Serial.println("Speak: "+text);
  if (waiting) awaitPlayable();  // 再生可能になるまで待つ
  if (!nowPlaying) {
    if (apiType == VoicevoxApiType::WebApiSlow) speakWebApiSlow(text);
    else if (apiType == VoicevoxApiType::WebApiFast) speakWebApiFast(text);
    else if (apiType == VoicevoxApiType::WebApiStream) speakWebApiStream(text);
    else if (apiType == VoicevoxApiType::RestApi) speakRestApi(text);
  }
}

// テキストを喋る WEB版VOICEVOX API（低速）
void VoicevoxTTS::speakWebApiSlow(String text) {
  /*
   * 指定したテキストから 1つの音声ファイル を作成するAPI
   * 処理完了までの状況をモニターする必要がある
   * 低速と書いてあるが、API KEYを指定すれば高速になるはず
   * Document: https://voicevox.su-shiki.com/su-shikiapis/ttsquest/
  */
  String url, audioUrl;
  bool mp3Ready = false;
  HtmlStatus hres;
  unsigned long timeout = 0;
  //StaticJsonDocument<1024> json;
  //DeserializationError error;

  // (1)音声合成をリクエストする
  url = endpointWebApiSlow + "?text="+URLEncode(text.c_str()) + "&speaker="+String(characterID);
  if (_apikeyWeb != "") url += "&key=" + _apikeyWeb;
  //hres = httpRequest(url, "GET");
  hres = httpGetJson(url, HttpMethod::GET, true);
  debugUrlPrint("VOICEVOX WebApiSlow Request", "GET "+url, hres.code, "", hres.html);  // デバッグ情報

  // レスポンス処理1
  if (hres.code == HTTP_CODE_OK) {
    // error = deserializeJson(*json, hres.html); // JSONをパースする
    // if (error) Serial.print(error.f_str());
    // if (!error && (*json)["success"]) {
    if ((*json)["success"]) {
      url = String((*json)["audioStatusUrl"].as<const char*>());
      audioUrl = String((*json)["mp3DownloadUrl"].as<const char*>());

      // (2) 音声合成ファイルの作成が完了するまで待つ
      timeout = millis() + VoicevoxGenerateTimeout;
      while (millis() < timeout) {
        //hres = httpRequest(url, "GET"); // 処理状況を問い合わせる
        hres = httpGetJson(url, HttpMethod::GET, true);
        debugUrlPrint("VOICEVOX WebApiSlow Wait", "GET "+url, hres.code, "", hres.html);  // デバッグ情報

        // レスポンス処理2
        if (hres.code == HTTP_CODE_OK) {
          //error = deserializeJson(*json, hres.html); // JSONをパースする
          //if (!error && (*json)["success"]) {
          if ((*json)["success"]) {
            if ((*json)["isAudioError"]) break;
            if ((*json)["isAudioReady"]) { // 正常に音声合成完了
              if (debug) Serial.println("mp3 url="+audioUrl);
              mp3Ready = true;
              break;
            }
          }
        }
        delay(VoicevoxStatusWaitTime);
      }
    }
  }
  debug_free_memory("ready to speak");

  // MP3ファイルをダウンロードして再生する
  if (mp3Ready) {
    if (debug) Serial.println("playUrlMP3: "+audioUrl);
    playUrlMP3(audioUrl);
  } else {
    Serial.println("VoicevoxTTS request failed.");
  }
}

// テキストを喋る WEB版VOICEVOX API（高速）　　※これは動かない
void VoicevoxTTS::speakWebApiFast(String text) {
  /*
   * 指定したテキストから 1つの音声ファイル を直接返すAPI
   * 処理完了までの状況をモニターする必要はない
   * このサーバーは現状Content-Lengthヘッダーを返さないので、HTTPClientが正常に動作しません。
   * AudioFileSourceHTTPStream2に直接URLを渡すのでエラー処理ができない。
   * なので、実際には動きません。
   * Document: https://voicevox.su-shiki.com/su-shikiapis/
   */
  String audioUrl;

  if (_apikeyWeb == "") return;
  audioUrl = endpointWebApiFast + "?text="+URLEncode(text.c_str()) + "&speaker="+String(characterID) + "&key="+_apikeyWeb;
  debugUrlPrint("VOICEVOX WebApiFast Request", "GET "+audioUrl, 0);  // デバッグ情報
  if (debug) Serial.println("playUrlWAV: "+audioUrl);
  playUrlWAV(audioUrl);  // WAVファイルをダウンロードして再生する

}

// テキストを喋る WEB版VOICEVOX API（Stream）
void VoicevoxTTS::speakWebApiStream(String text) {
  /*
   * 指定したテキストから 複数の音声ファイル を逐次作成するAPI
   * 音声ファイルごとに作成状況をモニターする必要がある
   * API KEYを指定しない場合でも低速動作で使えるはず
   * Document: https://github.com/ts-klassen/ttsQuestV3Voicevox
  */
  String url, audioUrl;
  bool mp3Ready = false;
  HtmlStatus hres;
  unsigned long timeout = 0;
  int i;
  //StaticJsonDocument<1024> json;
  //DeserializationError error;
  long audioCount = 0;

  // (1)音声合成をリクエストする
  url = endpointWebApiStrem + "?text="+URLEncode(text.c_str()) + "&speaker="+String(characterID);
  if (_apikeyWeb != "") url += "&key=" + _apikeyWeb;
  //hres = httpRequest(url, "GET");
  hres = httpGetJson(url, HttpMethod::GET, true);
  debugUrlPrint("VOICEVOX WebApiStream Request", "GET "+url, hres.code, "", hres.html);  // デバッグ情報

  // レスポンス処理1
  if (hres.code == HTTP_CODE_OK) {
    //error = deserializeJson(*json, hres.html); // JSONをパースする
    //if (!error && (*json)["success"]) {
    if ((*json)["success"]) {
      url = String((*json)["audioStatusUrl"].as<const char*>());
      audioUrl = String((*json)["mp3DownloadUrl"].as<const char*>());

      // (2) 音声ファイルの個数が判明するまで待つ
      timeout = millis() + VoicevoxGenerateTimeout;
      while (millis() < timeout) {
        //hres = httpRequest(url, "GET"); // 処理状況を問い合わせる
        hres = httpGetJson(url, HttpMethod::GET, true);
        debugUrlPrint("VOICEVOX WebApiStream Wait", "GET "+url, hres.code, "", hres.html);  // デバッグ情報

        // レスポンス処理2
        if (hres.code == HTTP_CODE_OK) {
          //error = deserializeJson(*json, hres.html); // JSONをパースする
          //if (!error && (*json)["success"]) {
          if ((*json)["success"]) {
            if ((*json)["isAudioError"]) break;
            if ((*json)["audioCount"] > 0) {
              int lastSlashPos = audioUrl.lastIndexOf('/');
              if (lastSlashPos != -1) {
                audioUrl = audioUrl.substring(0, lastSlashPos + 1);
                audioCount = (*json)["audioCount"];
                mp3Ready = true;
                if (debug) Serial.println("audioCount="+String(audioCount));
                if (debug) Serial.println("audioUrl="+audioUrl);
                break; //while
              }
              break;
            }
          }
        }
        delay(VoicevoxStatusWaitTime);
      }//while
    }
  }
  debug_free_memory("speakWebApiStream 2to3");

  // (3) 順番に音声ファイルを再生する
  if (mp3Ready && audioCount > 0) {
    for (i=0; i<audioCount; i++) {
      if (!awaitPlayable(300000)) continue; // 再生中なら再生が終わるまで待つ max 30s
      url = audioUrl + String(i) + ".mp3";

      // ファイルが作成されたか確認する
      timeout = millis() + VoicevoxGenerateTimeout;
      while (millis() < timeout) {
        //hres = httpRequest(url, "HEAD");
        hres = httpGetJson(url, HttpMethod::HEAD, false);
        if (debug) Serial.printf("MP3 (%d) HEAD %d\n" ,i, hres.code);
        if (hres.code == HTTP_CODE_OK) {
          // 作成完了なら再生する
          if (debug) Serial.println("playUrlMP3: "+url);
          playUrlMP3(url);
          break;
        }
        delay(VoicevoxStatusWaitTime);
      }
    }
  } else {
    Serial.println("VoicevoxTTS request failed.");
  }
}

// テキストを喋る VOICEVOX REST-API
void VoicevoxTTS::speakRestApi(String text){
  /*
   * ローカルPCで稼働するVOICEVOXの音声合成エンジンを使用する
   * LANからアクセスできるようにする方法
   * C:\Users\xxx\AppData\Local\Programs\VOICEVOX/run.exe --host 192.168.x.xx --port 50021
   * Document: http://192.168.x.xx:50021/docs
  */
  String url, audioUrl;
  bool mp3Ready = false;
  HtmlStatus hres;
  unsigned long timeout = 0;

  // 初期化
  for (int i=0; i<MAX_VOWEL_HISTORY; i++) {
    vowelHistories[i] = { VVVowel::null, 0 };
  }

  // (1)音声合成用のクエリを作成する
  url = endpointRestApi + "/audio_query?text="+URLEncode(text.c_str()) + "&speaker="+String(characterID);
  hres = httpGetJson(url, HttpMethod::POST, true, "");
  debugUrlPrint("VOICEVOX RestApi Request", "GET "+url, hres.code, "", hres.html);  // デバッグ情報

  // レスポンス処理1
  int accNum = 0;
  if (hres.code == HTTP_CODE_OK) {
    int index = 0;
    unsigned long timeline = 0;
    if ((*json).containsKey("accent_phrases")) {
      accNum = (*json)["accent_phrases"].size();
      // リップシンク用データを配列に保存する。JSONのフォーマットはmemo.txt参照
      int moraNum;
      if ((*json).containsKey("prePhonemeLength")) {
        timeline = (*json)["prePhonemeLength"].as<float>() * 1000;
      }
      for (int i=0; i<accNum; i++) {
        VVVowel vowel;
        uint16_t wait = 0;
        if ((*json)["accent_phrases"][i].containsKey("moras")) {
          moraNum = (*json)["accent_phrases"][i]["moras"].size();
          for (int j=0; j<moraNum; j++) {
            String jvowel = (*json)["accent_phrases"][i]["moras"][j]["vowel"];
            jvowel.toLowerCase();
            if (jvowel == "a") vowel = VVVowel::a;
            else if (jvowel == "i") vowel = VVVowel::i;
            else if (jvowel == "u") vowel = VVVowel::u;
            else if (jvowel == "e") vowel = VVVowel::e;
            else if (jvowel == "o") vowel = VVVowel::o;
            else if (jvowel == "n") vowel = VVVowel::n;
            else if (jvowel == "cl") vowel = VVVowel::u;  // ッ
            else {
              vowel = VVVowel::n;
              Serial.println("******* Unknown Vowel String \""+jvowel+"\"");
            }
            wait = (*json)["accent_phrases"][i]["moras"][j]["consonant_length"].as<float>() * 1000;
            wait += (*json)["accent_phrases"][i]["moras"][j]["vowel_length"].as<float>() * 1000;
            vowelHistories[index] = { vowel, timeline };
            timeline += wait;
            index ++;
          }
        }
        if (index >= (sizeof(vowelHistories)/sizeof(vowelHistories[0])-1)) break;
        if ((*json)["accent_phrases"][i].containsKey("pause_mora")) {
          if ((*json)["accent_phrases"][i]["pause_mora"].containsKey("vowel_length")) {
            wait = (*json)["accent_phrases"][i]["pause_mora"]["vowel_length"].as<float>() * 1000;
            vowelHistories[index] = { VVVowel::n, timeline };
            timeline += wait;
            index ++;
          }
        }
        if (index >= (sizeof(vowelHistories)/sizeof(vowelHistories[0])-1)) break;
      }
    }
    vowelHistories[index] = { VVVowel::null, timeline };
    // sp("index="+String(index));
    // for (int i=0; i<=index; i++) {
    //   sp("vowelHistories["+String(i)+"] = "+ String(vowelHistories[i].vowel) +" , "+ String(vowelHistories[i].timeline));
    // }
  }

  // (2)音声合成を実行し、再生する
  if (accNum > 0) {
    size_t bytesWritten = serializeJson(*json, postBuffer, preallocatePostSize+1);
    audioUrl = endpointRestApi + "/synthesis?&speaker="+String(characterID);
    if (debug) Serial.println("Post size="+String(bytesWritten));
    if (debug) Serial.println("playUrlWAV: "+audioUrl);
    playUrlWAV(audioUrl, true, postBuffer);
  }

}

// 指定URLから音声ファイルをダウンロードして再生する
void VoicevoxTTS::playUrl(String url, AudioFormat audioformat, bool post, char* data) {
  if (!nowPlaying) {
    nowPlaying = true;
    format = audioformat;
    file = new AudioFileSourceHTTPStream2(url.c_str(), post, data);
    if (file->size > 0) {
      if (useRootCACertificate) {
        file->setRootCA(rootCACertificate);  // ルート証明書
      } else {
        file->unsetRootCA();
      }
      buff = new AudioFileSourceBuffer(file, preallocateBuffer, preallocateBufferSize);
      playAudio(buff);
    } else {
      Serial.println("VoicevoxTTS open-url failed.");
      stopAudio();  // メモリ開放
    }
  }
}

// PROGMEMの音声ファイル再生する
void VoicevoxTTS::playProgmem(const unsigned char* data, size_t size, AudioFormat audioformat) {
  if (!nowPlaying) {
    nowPlaying = true;
    format = audioformat;
    filepg = new AudioFileSourcePROGMEM(data, size);
    buff = new AudioFileSourceBuffer(filepg, preallocateBuffer, preallocateBufferSize);
    playAudio(buff);
  }
}

// 再生開始
void VoicevoxTTS::playAudio(AudioFileSourceBuffer *buff) {
  sp("playAudio");
  if (format == AudioFormat::mp3) {
    mp3 = new AudioGeneratorMP3();
    mp3->begin(buff, _out);
  } else if (format == AudioFormat::wav) {
    wav = new AudioGeneratorWAV();
    wav->begin(buff, _out);
  }
  startAutoPlay();
}

// 再生停止（再生の停止とメモリ開放）
void VoicevoxTTS::stopAudio() {
  sp("stopAudio");
  if (mp3 != NULL) {
    if (format == AudioFormat::mp3) {
      mp3->stop();
      delete mp3;
      mp3 = NULL;
    } else if (format == AudioFormat::wav) {
      wav->stop();
      delete wav;
      wav = NULL;
    }
  }
  if (buff != NULL) {
    buff->close();
    delete buff;
    buff = NULL;
  }
  if (file != NULL) {
    file->close();
    delete file;
    file = NULL;
  }
  nowPlaying = false;
  for (int i=0; i<levelsCnt; i++) levels[i] = 0;
}

// タスク処理：自動音声再生
void taskAudioOutputLoop(void *args) {
  DriveContextTTS *ctx = reinterpret_cast<DriveContextTTS *>(args);
  VoicevoxTTS *vvtts = ctx->getVoicevoxTTS();
  vvtts->_nowPlayingVowel = VVVowel::null;
  unsigned long stams = millis();
  int vidx = 0;

  while (vvtts->nowAutoPlaying) {
    // 再生の継続処理
    if (vvtts->format == AudioFormat::mp3) {
      if(vvtts->mp3 != NULL) {
        if (vvtts->mp3->isRunning()) {
          if (!vvtts->mp3->loop()) break;
        } else break;
      }
    } else if (vvtts->format == AudioFormat::wav) {
      if(vvtts->wav != NULL) {
        if (vvtts->wav->isRunning()) {
          if (!vvtts->wav->loop()) break;
        } else break;
      }
    }
    // 音声レベル取得
    vvtts->levels[vvtts->levelsIdx] = abs(*(vvtts->_out)->getBuffer());
    vvtts->levelsIdx = (vvtts->levelsIdx + 1) % vvtts->levelsCnt;
    // 母音データ取得
    unsigned long pastms = millis() - stams;
    for (int i=vidx; i<MAX_VOWEL_HISTORY; i++) {
      if (vvtts->vowelHistories[i].timeline < pastms) {
        vvtts->_nowPlayingVowel = vvtts->vowelHistories[i].vowel;
        if (i < MAX_VOWEL_HISTORY-1 && vvtts->vowelHistories[i].vowel != VVVowel::null) {
          vvtts->_nowPlayingLength = vvtts->vowelHistories[i+1].timeline - vvtts->vowelHistories[i].timeline;
        } else {
          vvtts->_nowPlayingLength = 100;
        }
        vidx = i;
        if (vvtts->vowelHistories[i].vowel == VVVowel::null) break;
      } else {
        break;
      }
    }
  }
  vvtts->stopAudio();   // 再生停止
  vvtts->nowAutoPlaying = false;
  vTaskDelete(NULL);
}

// 自動音声再生を開始する
void VoicevoxTTS::startAutoPlay() {
  DriveContextTTS *ctx = new DriveContextTTS(this);
  if (!nowAutoPlaying) {
    nowAutoPlaying = true;
    // タスクを作成する
    xTaskCreateUniversal(
      taskAudioOutputLoop,  // Function to implement the task
      "taskAudioOutputLoop",// Name of the task
      4096,           // Stack size in words (2048だと落ちる)
      ctx,            // Task input parameter
      22,             // Priority of the task
      NULL,           // Task handle.
      CONFIG_ARDUINO_RUNNING_CORE);
  }
}

// 自動音声再生を終了する
void VoicevoxTTS::stopAutoPlay() {
  if (nowAutoPlaying) {
    // タスクを削除する（実際はタスク内で処理）
    nowAutoPlaying = false;
  }
  awaitPlayable(100);
}

// 再生可能になるまで待つ、タイムアウトあり
bool VoicevoxTTS::awaitPlayable(unsigned long timeout) {
  bool playable = false;
  unsigned long exittime = millis() + timeout;
  while (millis() < exittime) {
    if (!nowPlaying) {
      playable = true;
      break;
    }
    delay(1);
  }
  return playable;
}

// 今再生可能か？
bool VoicevoxTTS::isNowPlayable() {
  return !nowPlaying;
}

// 現在の再生中の音声レベルを求める
int VoicevoxTTS::getLevel() {
  int sum = 0;
  for (int i=0; i<levelsCnt; i++) {
    sum += levels[i];
  }
  return sum / levelsCnt;
}

// 現在の発話中の母音を求める
VowelData VoicevoxTTS::getVowel() {
  VowelData vd = { _nowPlayingVowel, _nowPlayingLength };
  return vd;
}

// APIの残りポイントを取得する
long VoicevoxTTS::getApiKeyPoint() {
  long point = 0;
  HtmlStatus hres;
  //StaticJsonDocument<128> json;
  //DeserializationError error;

  if (_apikeyWeb != "") {
    String url = endpointKeyPoint + "?key="+_apikeyWeb;
    //hres = httpRequest(url, "GET");
    hres = httpGetJson(url, HttpMethod::GET, true);
    debugUrlPrint("VOICEVOX KeyPoint", "GET "+url, hres.code, "", hres.html);  // デバッグ情報
    if (hres.code == HTTP_CODE_OK) {
      //error = deserializeJson(*json, hres.html); // JSONをパースする
      //if (!error && (*json)["success"] && (*json)["isApiKeyValid"]) {
      if ((*json)["success"] && (*json)["isApiKeyValid"]) {
        point = (*json)["points"];
      }
    }
  }
  return point;

}


// Called when a metadata event occurs (i.e. an ID3 tag, an ICY block, etc.
void VoicevoxTTS::MDCallback(void *cbData, const char *type, bool isUnicode, const char *string)
{
  const char *ptr = reinterpret_cast<const char *>(cbData);
  (void) isUnicode; // Punt this ball for now
  // Note that the type and string may be in PROGMEM, so copy them to RAM for printf
  char s1[32], s2[64];
  strncpy_P(s1, type, sizeof(s1));
  s1[sizeof(s1)-1]=0;
  strncpy_P(s2, string, sizeof(s2));
  s2[sizeof(s2)-1]=0;
  Serial.printf("METADATA(%s) '%s' = '%s'\n", ptr, s1, s2);
  Serial.flush();
}

// Called when there's a warning or error (like a buffer underflow or decode hiccup)
void VoicevoxTTS::StatusCallback(void *cbData, int code, const char *string)
{
  const char *ptr = reinterpret_cast<const char *>(cbData);
  // Note that the string may be in PROGMEM, so copy it to RAM for printf
  char s1[64];
  strncpy_P(s1, string, sizeof(s1));
  s1[sizeof(s1)-1]=0;
  Serial.printf("STATUS(%s) '%d' = '%s'\n", ptr, code, s1);
  Serial.flush();
}

// URLエンコードを行う
String VoicevoxTTS::URLEncode(const char* msg) {
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
void VoicevoxTTS::debugUrlPrint(String title, String url, int16_t code, String memo, String html) {
  if (debug) {
    if (title != "") Serial.printf("## %s ----------\n", title.c_str());
    Serial.printf("VOICEVOX code=%d url=%s\n", code, url.c_str());
    if (memo != "") Serial.printf("memo: %s\n", memo.c_str());
    if (html != "") Serial.printf("html: %s\n", html.c_str());
    Serial.println("");
  }
}

} //namespace
