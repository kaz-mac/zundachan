/*
  ServoChan.h
  ズンダチャン　スタックチャン風にサーボを扱うクラス

  ServoEasing使用（参考 https://github.com/ArminJo/ServoEasing）

  Copyright (c) 2024 Kaz  (https://akibabara.com/blog/)
  Released under the MIT license.
  see https://opensource.org/licenses/MIT
*/
#pragma once
#include <M5Unified.h>
#include <ServoEasing.h>      // ESP32Servoも必要。メインで.hppをincludeする必要がある点に注意。

// デバッグに便利なマクロ定義 --------
#define sp(x) Serial.println(x)
#define spn(x) Serial.print(x)
#define spf(fmt, ...) Serial.printf(fmt, __VA_ARGS__)

namespace servo_chan {

enum ServoType { None, PortA_Direct };  // サーボの接続方法

class ServoChan {
public:
  ServoType _servoType = ServoType::None;
  ServoEasing servo_x;
  ServoEasing servo_y;
  uint16_t servo_pin_x = 0;   // X軸のGPIO番号
  uint16_t servo_pin_y = 0;   // Y軸のGPIO番号
  uint16_t speedX = 60;
  uint16_t speedY = 60;
  float faceX = 1.0;
  float faceY = 1.0;

  float homeAngleX    = 90;
  float movableAngleX = 45 * 2;
  float homeAngleY    = 90;
  float movableAngleY = 45 * 2;

  ServoChan();
  ~ServoChan() = default;
  //~ServoChan();

  void connectGpioXY(ServoType type, int16_t portax=-1, int16_t portay=-1);   // サーボのGPIO設定を行う 
  void setSpeedDefault(float speed);  // デフォルトのスピードを設定する
  void moveAngleXY(float ax, float ay, bool await=false); // X,Yの角度に即座に移動する
  void headPosition(float px, float py, bool await=false);  // 頭の角度を移動する(範囲-1.0～1.0)


};//class

/*
// from M5Stack-Avatar (https://github.com/meganetaaan/m5stack-avatar)
class DriveContextSrv {
 private:
  ServoChan *srvcn;
 public:
  DriveContextSrv() = delete;
  explicit DriveContextSrv(ServoChan *srvcn);
  ~DriveContextSrv() = default;
  DriveContextSrv(const DriveContextSrv &other) = delete;
  DriveContextSrv &operator=(const DriveContextSrv &other) = delete;
  ServoChan *getServoChan();
};
*/

}//namespace