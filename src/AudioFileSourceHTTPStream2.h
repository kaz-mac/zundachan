/*
  AudioFileSourceHTTPStream
  Connect to a HTTP based streaming service
  
  Copyright (C) 2017  Earle F. Philhower, III

  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/
/*
  AudioFileSourceHTTPStream の修正版
  ・httpsに対応
  ・http/https自動判定
  ・ルート証明書の登録、証明書を検証しない、両モード対応　（ただし動作未確認）

  Copyright (C) 2017  Earle F. Philhower, III
  Modified by Kaz (https://akibabara.com/blog/)
  Released under the GNU General Public License

  オリジナル
  https://github.com/earlephilhower/ESP8266Audio/blob/master/src/AudioFileSourceHTTPStream.h
*/

// デバッグに便利なマクロ定義 --------
#define sp(x) Serial.println(x)
#define spn(x) Serial.print(x)
#define spf(fmt, ...) Serial.printf(fmt, __VA_ARGS__)

#if defined(ESP32) || defined(ESP8266)
#pragma once

#include <Arduino.h>
#ifdef ESP32
  #include <HTTPClient.h>
  #include <WiFiClient.h>
  #include <WiFiClientSecure.h>
#else
  #include <ESP8266HTTPClient.h>
#endif
#include "AudioFileSource.h"

class AudioFileSourceHTTPStream2 : public AudioFileSource
{
  friend class AudioFileSourceICYStream;

  public:
    AudioFileSourceHTTPStream2();
    AudioFileSourceHTTPStream2(const char *url, bool post=false, char* data=nullptr);
    virtual ~AudioFileSourceHTTPStream2() override;
    
    virtual bool open(const char *url) override;
    virtual uint32_t read(void *data, uint32_t len) override;
    virtual uint32_t readNonBlock(void *data, uint32_t len) override;
    virtual bool seek(int32_t pos, int dir) override;
    virtual bool close() override;
    virtual bool isOpen() override;
    virtual uint32_t getSize() override;
    virtual uint32_t getPos() override;
    bool SetReconnect(int tries, int delayms) { reconnectTries = tries; reconnectDelayMs = delayms; return true; }
    void useHTTP10 () { http.useHTTP10(true); }
    void setRootCA(const char* root_ca);
    void unsetRootCA();

    enum { STATUS_HTTPFAIL=2, STATUS_DISCONNECTED, STATUS_RECONNECTING, STATUS_RECONNECTED, STATUS_NODATA };
    int size;
    const char* rootCACertificate = NULL;
    bool useRootCACertificate = false;
    bool sslEnabled = false;
    bool postEnabled = false;
    char* postData = nullptr;

  private:
    virtual uint32_t readInternal(void *data, uint32_t len, bool nonBlock);
    WiFiClient client;
    WiFiClientSecure sclient;
    HTTPClient http;
    int pos;
    int reconnectTries;
    int reconnectDelayMs;
    char saveURL[128];
};

#endif

