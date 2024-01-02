/*
  AudioFileSourceHTTPStream
  Streaming HTTP source

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
  https://github.com/earlephilhower/ESP8266Audio/blob/master/src/AudioFileSourceHTTPStream.cpp
*/

#if defined(ESP32) || defined(ESP8266)

#include "AudioFileSourceHTTPStream2.h"

AudioFileSourceHTTPStream2::AudioFileSourceHTTPStream2()
{
  pos = 0;
  reconnectTries = 0;
  saveURL[0] = 0;
  rootCACertificate = NULL;
  sslEnabled = false;
}

AudioFileSourceHTTPStream2::AudioFileSourceHTTPStream2(const char *url, bool post, char* data)
{
  saveURL[0] = 0;
  reconnectTries = 0;
  rootCACertificate = NULL;
  sslEnabled = false;
  postEnabled = post;
  postData = (postEnabled) ? data : nullptr;
  open(url);
}

bool AudioFileSourceHTTPStream2::open(const char *url)
{
  int code;
  pos = 0;
  size = 0;
  if (strncmp(url, "https://", 8) == 0) {
    sslEnabled = true;
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
#ifndef ESP32
  http.setFollowRedirects(HTTPC_FORCE_FOLLOW_REDIRECTS);
#endif
  if (postEnabled) {
    code = http.POST((uint8_t*)postData, strlen(postData));
  } else {
    code = http.GET();
  }
  if (code != HTTP_CODE_OK) {
    http.end();
    cb.st(STATUS_HTTPFAIL, PSTR("Can't open HTTP request"));
    return false;
  }
  size = http.getSize();
  if (!postEnabled && sizeof(saveURL) < strlen(url)) {
    strncpy(saveURL, url, sizeof(saveURL));
    saveURL[sizeof(saveURL)-1] = 0;
  }
  return true;
}

AudioFileSourceHTTPStream2::~AudioFileSourceHTTPStream2()
{
  http.end();
}

uint32_t AudioFileSourceHTTPStream2::read(void *data, uint32_t len)
{
  if (data==NULL) {
    audioLogger->printf_P(PSTR("ERROR! AudioFileSourceHTTPStream2::read passed NULL data\n"));
    return 0;
  }
  return readInternal(data, len, false);
}

uint32_t AudioFileSourceHTTPStream2::readNonBlock(void *data, uint32_t len)
{
  if (data==NULL) {
    audioLogger->printf_P(PSTR("ERROR! AudioFileSourceHTTPStream2::readNonBlock passed NULL data\n"));
    return 0;
  }
  return readInternal(data, len, true);
}

uint32_t AudioFileSourceHTTPStream2::readInternal(void *data, uint32_t len, bool nonBlock)
{
retry:
  if (!http.connected()) {
    cb.st(STATUS_DISCONNECTED, PSTR("Stream disconnected"));
    http.end();
    for (int i = 0; i < reconnectTries; i++) {
      char buff[64];
      sprintf_P(buff, PSTR("Attempting to reconnect, try %d"), i);
      cb.st(STATUS_RECONNECTING, buff);
      delay(reconnectDelayMs);
      if (open(saveURL)) {
        cb.st(STATUS_RECONNECTED, PSTR("Stream reconnected"));
        break;
      }
    }
    if (!http.connected()) {
      cb.st(STATUS_DISCONNECTED, PSTR("Unable to reconnect"));
      return 0;
    }
  }
  if ((size > 0) && (pos >= size)) return 0;

  WiFiClient *stream = http.getStreamPtr();
  delay(1);

  // Can't read past EOF...
  if ( (size > 0) && (len > (uint32_t)(pos - size)) ) len = pos - size;

  if (!nonBlock) {
    int start = millis();
    while ((stream->available() < (int)len) && (millis() - start < 500)) yield();
  }

  size_t avail = stream->available();
  if (!nonBlock && !avail) {
    cb.st(STATUS_NODATA, PSTR("No stream data available"));
    http.end();
    if (strlen(saveURL) > 0) goto retry;
  }
  if (avail == 0) return 0;
  if (avail < len) len = avail;

  int read = stream->read(reinterpret_cast<uint8_t*>(data), len);
  pos += read;
  return read;
}

bool AudioFileSourceHTTPStream2::seek(int32_t pos, int dir)
{
  audioLogger->printf_P(PSTR("ERROR! AudioFileSourceHTTPStream2::seek not implemented!"));
  (void) pos;
  (void) dir;
  return false;
}

bool AudioFileSourceHTTPStream2::close()
{
  http.end();
  postEnabled = false;
  return true;
}

bool AudioFileSourceHTTPStream2::isOpen()
{
  return http.connected();
}

uint32_t AudioFileSourceHTTPStream2::getSize()
{
  return size;
}

uint32_t AudioFileSourceHTTPStream2::getPos()
{
  return pos;
}

// ルート証明書をセットする
void AudioFileSourceHTTPStream2::setRootCA(const char* root_ca) {
  rootCACertificate = root_ca;
  useRootCACertificate = true;
}

// ルート証明書を無効にする
void AudioFileSourceHTTPStream2::unsetRootCA() {
  rootCACertificate = NULL;
  useRootCACertificate = false;
}

#endif
