/*
  ServoChan.cpp
  ズンダチャン　スタックチャン風にサーボを扱うクラス

  ServoEasing使用（参考 https://github.com/ArminJo/ServoEasing）

  Copyright (c) 2024 Kaz  (https://akibabara.com/blog/)
  Released under the MIT license.
  see https://opensource.org/licenses/MIT
*/
#include "ServoChan.h"
namespace servo_chan {

// コンストラクタ
ServoChan::ServoChan() {
  _servoType = ServoType::None;
}

// サーボのGPIO設定を行う
void ServoChan::connectGpioXY(ServoType type, int16_t portax, int16_t portay) {
  _servoType = type;

  // Core2想定 Port.A に直接サーボを接続する場合
  if (type == ServoType::PortA_Direct) {
    // GPIOポートの自動設定（未指定の場合）
    servo_pin_x = (portax < 0) ? M5.Ex_I2C.getSCL() : portax;   // Core2=33
    servo_pin_y = (portay < 0) ? M5.Ex_I2C.getSDA() : portay;   // Core2=32
    Serial.printf("Servo GPIO Auto Setting: X=%d Y=%d\n", servo_pin_x, servo_pin_y);

    // GPIOを割り当てる
    M5.Ex_I2C.release();  // Port.Aの外部I2Cを開放する（）
    if (servo_x.attach(servo_pin_x, 90, DEFAULT_MICROSECONDS_FOR_0_DEGREE, DEFAULT_MICROSECONDS_FOR_180_DEGREE)) {
      Serial.println("Error attaching servo x");
    }
    if (servo_y.attach(servo_pin_y, 90, DEFAULT_MICROSECONDS_FOR_0_DEGREE, DEFAULT_MICROSECONDS_FOR_180_DEGREE)) {
      Serial.println("Error attaching servo y");
    }
  }

  // デフォルト設定
  if (type == ServoType::PortA_Direct) {
    servo_x.setEasingType(EASE_QUADRATIC_IN_OUT);
    servo_y.setEasingType(EASE_QUADRATIC_IN_OUT);
    setSpeedForAllServos(60);
  }
}

// デフォルトのスピードを設定する
void ServoChan::setSpeedDefault(float speed) {
    //setSpeedForAllServos(speed);
    speedX = speed;
    speedY = speed;
}

// X,Yの角度に移動する
void ServoChan::moveAngleXY(float ax, float ay, bool await) {
  bool opt = await ? DO_NOT_START_UPDATE_BY_INTERRUPT : START_UPDATE_BY_INTERRUPT;
  servo_x.startEaseTo(ax, speedX, opt);
  servo_y.startEaseTo(ay, speedY, opt);
  //spf("Servo x=%f y=%f\n", ax, ay);
  if (await) {
    synchronizeAllServosStartAndWaitForAllServosToStop();
  }
}

// 頭の向きを移動する(範囲-1.0～1.0)
void ServoChan::headPosition(float px, float py, bool await) {
  //spf("headPosition x=%f y=%f\n", px, py);
  float ax = homeAngleX + (movableAngleX / 2.0) * px;
  float ay = homeAngleY + (movableAngleY / -2.0) * py;
  moveAngleXY(ax, ay, await);
}

/*
// タスク処理：自動音声再生
void taskServoLoop(void *args) {
  DriveContextSrv *ctx = reinterpret_cast<DriveContextSrv *>(args);
  ServoType *srv = ctx->getServoType();

  while (vvtts->nowAutoPlaying) {
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
    delay(5);
  }
  srv->stopAudio();   // 再生停止
  srv->nowAutoPlaying = false;
  vTaskDelete(NULL);
}

// 自動音声再生を開始する
void VoicevoxTTS::startAutoPlay() {
  DriveContextSrv *ctx = new DriveContextSrv(this);
  if (!nowAutoPlaying) {
    nowAutoPlaying = true;
    // タスクを作成する
    xTaskCreateUniversal(
      taskServoLoop,  // Function to implement the task
      "taskServoLoop",// Name of the task
      1024,           // Stack size in words
      ctx,            // Task input parameter
      5,             // Priority of the task
      NULL,           // Task handle.
      CONFIG_ARDUINO_RUNNING_CORE);
  }
}
*/

} //namespace