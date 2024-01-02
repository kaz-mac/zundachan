/*
  function.h
  ズンダチャン　クラス化するまでもないちょっとした関数

  Copyright (c) 2024 Kaz  (https://akibabara.com/blog/)
  Released under the MIT license.
  see https://opensource.org/licenses/MIT
*/
#pragma once

#include <WiFi.h>
#include <ESPmDNS.h>

// Wi-Fi接続する
bool wifiConnect() {
  bool stat = false;
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Wifi connecting.");
    for (int j=0; j<10; j++) {
      WiFi.disconnect();
      WiFi.mode(WIFI_STA);
      WiFi.begin(WIFI_SSID, WIFI_PASS);  //  Wi-Fi APに接続
      for (int i=0; i<10; i++) {
        if (WiFi.status() == WL_CONNECTED) break;
        Serial.print(".");
        delay(500);
      }
      if (WiFi.status() == WL_CONNECTED) {
        Serial.println("connected!");
        Serial.println(WiFi.localIP());
        stat = true;
        break;
      } else {
        Serial.println("failed");
        WiFi.disconnect();
      }
    }
  }
  return stat;
}

// mDNSにホスト名を登録する
bool mdnsRegister(char* hostname) {
  for (int i=0; i<100; i++) {
    if (WiFi.status() == WL_CONNECTED) break;
    delay(50);
  }
  if (WiFi.status() == WL_CONNECTED) {
    if (MDNS.begin(hostname)) {
      Serial.println("mDNS registered. http://"+String(hostname)+".local/");
      return true;
    }
  }
  Serial.println("mDNS failed.");
  return false;
}

// BEEP音を鳴らす
void beep(unsigned long duration=100) {
  M5.Speaker.tone(2000, duration);
  delay(duration);
}

// タッチスクリーンのボタン定義（robo8080さん作 MIT LICENSE）
// https://github.com/robo8080/AI_StackChan2/blob/main/M5Unified_AI_StackChan/src/main.cpp
struct box_t {
  int x, y, w, h;
  int touch_id = -1;
  //int k = 0;
  //unsigned long tm = 0;

  void setupBox(int x, int y, int w, int h) {
    this->x = x;
    this->y = y;
    this->w = w;
    this->h = h;
  }

  bool contain(int x, int y) const {
    return this->x <= x && x < (this->x + this->w)
        && this->y <= y && y < (this->y + this->h);
  }
};


