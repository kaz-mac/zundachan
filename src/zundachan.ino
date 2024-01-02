/*
  zundachan.ino
  ズンダチャン　メインプログラム

  Copyright (c) 2024 Kaz  (https://akibabara.com/blog/)
  Released under the MIT license.
  see https://opensource.org/licenses/MIT
*/
#include <M5Unified.h>
#include "CharacterConfig.h"  // キャラクター設定情報
#include "ApiKey.h"   // API KEYなどの機密情報

// キャラクター設定（共通）
char* hostNames[] = { "zunda", "metan" };   // ホスト名
char* characterNames[] = { "ずんだもん", "四国めたん" };  // キャラクターの名前
int characterMaxNum = 2;

// アプリケーション設定
#define SOUND_VOLUME 128 // 音量（max 255）
#define SERVO_SPEED_SLOW 20   // サーボのスピード 低速時
#define SERVO_SPEED_FAST 120   // サーボのスピード 高速時
#define SERVO_SPEED_VFAST 240   // サーボのスピード 超高速時

// ツール類
#include "tools.h"    // 作業用のプログラム
#include "function.h"  // メインより使うサブルーチン

// アバター関連
#include "Zundavatar.h"   // アバタークラス
using namespace zundavatar;
Zundavatar avatar;

// アバター画像データ・キャラクター別
#if CHARACTER_NO == 0
#include "image_zundamon.h"   // 画像データ　ずんだもん
#elif CHARACTER_NO == 1
#include "image_metan.h"   // 画像データ　四国めたん
#endif

// WiFi関連
#include <WiFi.h>

// オーディオ出力関連
#include "AudioOutputM5Speaker.h"   // M5UnifiedでESP8266Audioを使うためのクラス
static constexpr uint8_t m5spk_virtual_channel = 0;
AudioOutputM5Speaker out(&M5.Speaker, m5spk_virtual_channel);

// 音声合成関連
#include "VoicevoxTTS.h"  // VOICEVOXで音声合成を行い、再生するためのクラス
using namespace voicevox_tts;
VoicevoxTTS tts;

// 音声データの読み込み
#if CHARACTER_NO == 0
#include "sound.h"   // 音声データ　ずんだもん
#elif CHARACTER_NO == 1
#include "sound.h"   // 音声データ　ずんだもん
#endif

// ChatGPT関連
#include "ChatGPT.h"
using namespace chat_gpt;
ChatGPT gpt;

// サーボ関連
#include <ServoEasing.hpp>      
#include "ServoChan.h"
using namespace servo_chan;
ServoChan servo;

// Webサーバー関連
#include "WebInterface.h"
using namespace web_interface;
WebInterface web;

// 宣言

// グローバル変数
int characterNo = CHARACTER_NO;
bool singleMode = true;   // 一人会話モード。2台のズンダチャンでおしゃべりする場合はfalseにする

// 設定
#define VOICEVOX_RESTAPI_ENDPOINT "http://192.168.x.xx:50021"    // VOICEVOX RESR-APIのエンドポイント

// タッチパネル
static box_t btnBody;
static box_t btnHead;

// デバッグに便利なマクロ定義 --------
#define sp(x) Serial.println(x)
#define spn(x) Serial.print(x)
#define spf(fmt, ...) Serial.printf(fmt, __VA_ARGS__)
#define lp(x) M5.Lcd.println(x)
#define lpn(x) M5.Lcd.print(x)
#define lpf(fmt, ...) M5.Lcd.printf(fmt, __VA_ARGS__)
#define array_length(x) (sizeof(x) / sizeof(x[0]))

// ====================================================================================

// 初期化
void setup() {
  auto cfg = M5.config();
  cfg.external_spk = true;    /// use external speaker (SPK HAT / ATOMIC SPK)
  M5.begin(cfg); 
  sp("Start");
  int i;
  debug_free_memory("Start");

  // タッチパネルの仮想ボタンの設定
  btnBody.setupBox(100, 0, 120, 210);   // 体全体
  btnHead.setupBox(60, 0, 200, 100);    // 頭

  // サーボの設定
  servo.connectGpioXY(ServoType::PortA_Direct); // Port.A直結 GPIO番号自動設定
  servo.setSpeedDefault(SERVO_SPEED_SLOW);  // 移動速度 default 60
  servo.movableAngleX  = 10 * 2;  // X軸の可動範囲
  servo.movableAngleY  = 20 * 2;  // Y軸の可動範囲

  // Aボタンを押しながら起動したら、サーボ調整ツールを実行する
  if (M5.BtnA.isPressed()) { 
    beep();
    extend_servo_adjust();
  }

  // ディスプレイの設定
  M5.Lcd.init();
  M5.Lcd.setColorDepth(16);
  M5.Lcd.fillScreen(TFT_WHITE);

  // WiFi接続
  wifiConnect();
  mdnsRegister(hostNames[characterNo]);  // mDNSにホスト名を登録する

  // スピーカーの設定
  auto spk_cfg = M5.Speaker.config(); // https://docs.m5stack.com/ja/api/m5unified/m5unified_appendix
  spk_cfg.sample_rate = 96000; // default:64000 (64kHz)  e.g. 48000 , 50000 , 80000 , 96000 , 100000 , 128000 , 144000 , 192000 , 200000
  spk_cfg.task_pinned_core = APP_CPU_NUM;
  M5.Speaker.config(spk_cfg);
  M5.Speaker.begin();
  M5.Speaker.setVolume(SOUND_VOLUME);  // 0-255 default:64
  M5.Speaker.setChannelVolume(m5spk_virtual_channel, 255);

  // 音声合成の設定
  tts.usePSRAM(true);
  tts.setApikey(VOICEVOX_APIKEY);
  //tts.init(&out, VoicevoxApiType::WebApiSlow);    // VOICEVOX WEB版(低速)を使う場合はこちら
  //tts.init(&out, VoicevoxApiType::WebApiStream);    // VOICEVOX WEB版(WebApiStream)を使う場合はこちら
  tts.init(&out, VoicevoxApiType::RestApi);                               // VOICEVOX RESR-APIを使う場合はこちら
  tts.setEndpoint(VoicevoxApiType::RestApi, VOICEVOX_RESTAPI_ENDPOINT);   // VOICEVOX RESR-APIを使う場合はこちら
  tts.changeCharacter(VOICEVOX_SPEAKER_NO);   // 話者設定

  // アバターの設定
  avatar.usePSRAM(true);
  String tableNames[]   = { "body", "rhand", "lhand", "eyebrow", "eye", "mouth" };  // 部位名（imgTablesと順番を揃える）
  uint16_t* imgTables[] = { imgTableBody, imgTableRhand, imgTableLhand, imgTableEyebrow, imgTableEye, imgTableMouth };  // 部位ごとにリスト化した表（テーブル）
  avatar.setImageData(imgInfo, tableNames, imgTables, 6); // 画像データを登録する
  avatar.useAntiAliases = false;  // アンチエイリアス
  avatar.mirrorImage = false;//反転はバグあり(CHARACTER_NO != 0);      // 左右反転
  avatar.setDrawDisplay(&M5.Lcd, 40,0, TFT_WHITE); // アバターの表示先を設定する（出力先, x, y, 背景色）
  //avatar.changeDrawPosition(40, 0); // アバターの表示先を変更する（x, y）
  avatar.debugtable();
  debug_free_memory("after avater setting");

  // 表示するアバターのパーツを決める
  avatar.changeParts("body", 0);    // 体
  avatar.changeParts("rhand", 0);   // 右腕
  avatar.changeParts("lhand", 0);   // 左腕
  avatar.changeParts("eyebrow", 0); // 眉毛
  avatar.changeParts("eye", 1);     // 目
  avatar.changeParts("mouth", 1);   // 口
  avatar.drawAvatar(); // アバター全体表示

  // アバターのまばたきとリップシンクの設定
  avatar.setBlink("eye", 1, 0);   // まばたき用のインデックス番号を設定する
  avatar.setLipsync("mouth", 2, 3, 4, 5, 6, 1);   // リップシンク用のインデックス番号を設定する
  //avatar.startAutoBlink();  // 自動まばたきスタート（タスク実行）
  avatar.startAutoLipsync();  // リップシンクをスタート（タスク実行）

  // ChatGPTの設定
  gpt.init(OPENAI_APIKEY);
  gpt.setMainValues(characterMaxNum, characterNo, characterNames, hostNames);   // キャラクター情報を渡す
  // gpt.addHistory(-1, "あなたの名前を教えてください");
  // String resMessage = gpt.requestChat(true);
  // tts.speak(resMessage, true);

  // Webサーバーの設定
  web.webSetup(&tts, &gpt);   // Webサーバーの初期化と開始、各クラスのポインタを渡す
  web.setMainValues(characterMaxNum, characterNo, characterNames, hostNames);   // キャラクター情報を渡す

  // 起動サウンドを鳴らす「のだー」　（データはsound.hに格納）
  tts.playProgmem(soundFlashData[0], soundFlashSize[0], AudioFormat::wav);  //内蔵サウンド「のだー」
  delay(1000);
}

// ====================================================================================

// 状態
enum Mode {
  None,   // 未定義
  Free,   // 自由モード
  Touch,  // タッチモード
  Nade    // なでモード
};

// 各モード別の状態をまとめた構造体
struct Bstat {
  bool now = false;
  bool old = false;
  unsigned long tm = 0;
  int k = 0;
};

// メインループ
void loop() {
  M5.update();
  web.webLoop();
  unsigned long sta, tm0;
  bool refreshAvatar = false;
  Mode nextstat = Mode::None;
  int i, kp;

  // static変数
  static Mode stat = Mode::Free;
  static Mode oldstat = Mode::None;
  static Bstat blink, lipsync, freeservo, freetalk;
  static Bstat mTouch, mNade;

  // タッチパネルの状態を取得
  auto touchCount = M5.Touch.getCount();
  auto t = M5.Touch.getDetail();

  // Aボタン：ランダムサーボのオン/オフ
  if (M5.BtnA.wasPressed()) {
    beep();
    freeservo.now = !freeservo.now;
    if (freeservo.now) {
      stat = Mode::Free;    // 自由モードに戻す
    } else {
      servo.headPosition(0, 0);   // 頭をホームポジションに戻す
    }
  }

  // タッチパネル
  if (touchCount > 0) {
    if (btnBody.contain(t.x, t.y) && stat == Mode::Free) {  // 体をタッチ、自由モードのとき
      mTouch.now = true;
      mTouch.k = 0;
      mNade.now = false;
      mNade.k = 0;
      stat = Mode::Touch;   // タッチモードに移行
    } else if (btnHead.contain(t.x, t.y) && stat == Mode::Touch) {  // 頭をタッチ、タッチモードのとき
      mNade.k ++;
      if (mNade.k > 100) {
        mNade.now = true;
        mNade.k = 0;
        mTouch.now = false;
        mTouch.k = 0;
        stat = Mode::Nade;   // なでに移行
      }
    }
  }

  // Cボタン：自動おしゃべりのオン/オフ
  if (M5.BtnC.wasPressed() || web.notice.pressButton == 3) {
    web.notice.pressButton = 0;
    freetalk.now = !freetalk.now;
    beep();
    if (!freetalk.now) {
      tts.stopAutoPlay();   // 自動再生中なら中断する
    }
  }

  // 音量の変更
  if (web.notice.changeVolume != -1) {
    if (web.notice.changeVolume >= 0 || web.notice.changeVolume <= 255) {
      M5.Speaker.setVolume(web.notice.changeVolume);  // 0-255 default:64
      beep();
    }
    web.notice.changeVolume = -1;
  }

  // シングルモードの切り替え
  if (web.notice.singleModeChange != -1) {
    singleMode = (web.notice.singleModeChange == 1);
if(singleMode) sp("singlemode on");
else sp("singlemode off");
    web.notice.singleModeChange = -1;
  }

  // モードに変化があったら新しいモードに遷移する（変化があったときに一度だけ実行する）
  if (stat != oldstat) {

    // タッチモードから抜けたとき、スケールを変更していた場合は戻す
    if (oldstat == Mode::Touch && stat != Mode::Touch) {
      avatar.scaleBodyCanvasY = 1.0;
      avatar.scaleBodyCanvasX = 1.0;
      refreshAvatar = true;
    }

    switch (stat) {
    case Mode::Free : //----- 自由モード -----
      sp("Enter Free mode");
      // まばたきとリップシンク
      blink.now = true;     // 自動まばたき 有効化
      lipsync.now = true;   // 自動リップシンク オン
      blink.tm = 0;
      lipsync.tm = 0;
      avatar.startAutoBlink();    // 自動まばたきスタート（タスク実行）
      avatar.startAutoLipsync();  // リップシンクをスタート（タスク実行）
      // アバターの表情
      avatar.changeParts("eyebrow", 0); // 眉毛　普通
      avatar.changeParts("eye", 1);     // 目　開き
      avatar.changeParts("mouth", 1);   // 口　閉じ
      avatar.changeParts("rhand", 0);   // 右腕　下げ
      avatar.changeParts("lhand", 0);   // 左腕　下げ
      // ランダムサーボとランダムトーク
      freeservo.now = true;
      freeservo.tm = 0;
      freetalk.now = true;
      freetalk.tm = 0;
      freetalk.k = 0;
      break;

    case Mode::Touch : //----- タッチモード -----
      sp("Enter Touch mode");
      if (oldstat == Mode::Free) {
        tts.stopAutoPlay();   // 再生中なら中断する
        tts.playProgmem(soundFlashData[0], soundFlashSize[0], AudioFormat::wav);  //内蔵サウンド「のだー」
        //tts.speak("なのだ");
      }
      servo.setSpeedDefault(SERVO_SPEED_FAST);  // 高速
      servo.headPosition(0, 1.0, true);           // 頭を上げて、完了まで待つ
      servo.setSpeedDefault(SERVO_SPEED_SLOW);  // 低速に戻す
      avatar.changeParts("eye", 2);     // 目　左（カメラ目線）
      avatar.setBlink("eye", 2, 0);     // まばたき　同上
      avatar.changeParts("mouth", 1);   // 口　閉じ
      //lipsync.now = false;   // 自動リップシンク オフ
      freeservo.now = false;
      freetalk.now = false;
      mTouch.k = 0;
      if (mTouch.tm == 0) mTouch.tm = millis() + 10000;
      break;

    case Mode::Nade : //----- なでモード -----
      sp("Enter Nade mode");
      // 表情変更
      avatar.stopAutoBlink();    // 自動まばたき終了（タスク終了）
      //avatar.stopAutoLipsync();  // リップシンク終了（タスク終了）
      avatar.changeParts("eye", 5);     // 目　＞＜
      avatar.changeParts("mouth", 0);   // 口　普通
      // 喋る
      tts.stopAutoPlay();   // 再生中なら中断する
      tts.playProgmem(soundFlashData[1], soundFlashSize[1], AudioFormat::wav);  //内蔵サウンド「くすぐったいのだ」
      // バンザイのループ処理
      mNade.tm = millis() + 2000;
      servo.setSpeedDefault(SERVO_SPEED_VFAST);  // サーボ　超高速
      while (millis() < mNade.tm) {
        kp = mNade.k++ % 4;
        if (kp == 0) {
          if (characterNo == 0) {  // ずんだもん
            avatar.changeParts("rhand", 1);   // 右腕　上げ
            avatar.changeParts("lhand", 1);   // 左腕　上げ
          } else if (characterNo == 1) { // めたん
            avatar.changeParts("rhand", 2);   // 右腕　指差し
            avatar.changeParts("lhand", 0);   // 左腕　普通
          }
          refreshAvatar = true;
        } else if (kp == 2) {
          avatar.changeParts("rhand", 0);   // 右腕　下げ
          avatar.changeParts("lhand", 0);   // 左腕　下げ
          refreshAvatar = true;
        } else if (kp == 1) {
          servo.headPosition(-1.0, 1.0);    // 頭を右に振る
        } else if (kp == 3) {
          servo.headPosition(1.0, 1.0);    // 頭を左に振る
        }
        if (refreshAvatar) avatar.drawAvatar(); // アバターを再描画
        delay(50);
      }
      servo.setSpeedDefault(SERVO_SPEED_SLOW);  // サーボ　低速に戻す
      // 変更したものを元に戻す（その他タッチモードで設定するものは次回のループ時に行う）
      avatar.changeParts("rhand", 0);   // 右腕　普通
      avatar.changeParts("lhand", 0);   // 左腕　普通
      avatar.changeParts("eye", 2);     // 目　左（カメラ目線）
      avatar.changeParts("mouth", 1);   // 口　閉じ
      refreshAvatar = true;
      avatar.startAutoBlink();    // 自動まばたきスタート（タスク実行）
      nextstat = Mode::Touch;   // 次回、タッチモードに戻る
      mTouch.tm = millis() + 2000;  // 2秒後にタッチモードを抜ける
      mNade.now = false;
      //tts.awaitPlayable();  // 再生が終わってなかったら終わるまで待つ
      break;

    }//switch
    oldstat = stat;
  }

  // 自動まばたき実行中に目をキョロキョロさせる
  if (blink.now && avatar.autoBlink && (stat == Mode::Free)) {
    if (blink.tm < millis()) {
      const uint8_t eyePattern[] = { 2, 3, 2, 1 };  // 1=中 2=左 3=右
      avatar.setBlink("eye", eyePattern[blink.k++ % 4], 0);
      blink.tm = millis() + 500 + random(500, 2000);  // 次の変更タイミング
    }
  }

  // ランダムサーボ
  if (freeservo.now) {
    if (freeservo.tm < millis()) {
      float fx = (random(2001) - 1000) / 1000.0;
      float fy = (random(2001) - 1000) / 1000.0;
      servo.headPosition(fx, fy); // 頭の向きを変更
      freeservo.tm = millis() + 1000 + random(500, 2000);  // 次の変更タイミング
    }
  }

  // 自動おしゃべり・デモ
  if (0 && freetalk.now) {
    if (tts.isNowPlayable()) {  // 再生可能な状態か？
      kp = freetalk.k++ % 3;
      if (kp == 0) tts.speak("ずんだ餅にかかわることはだいたい好き。将来の夢はずんだ餅のさらなる普及。あともうちょっと発言力を高めたい。");
      if (kp == 1) tts.speak("高評価・チャンネル登録、お願いしますなのだ。");
      if (kp == 2) tts.speak("ずんだ餅の精。ずんだアローに変身することができる。やや不幸属性が備わっており、ないがしろにされることもしばしば。趣味：ずんだ餅にかかわることはだいたい好き。 将来の夢：ずんだ餅のさらなる普及。あともうちょっと発言力を高めたい。特技：ずんだアローに変身すること。");
    }
  }

  // AI同士でチャット
  static String resMessage = "";
  int friendCharacterNo = (characterNo == 0) ? 1 : 0;   // 会話相手のキャラクター番号　TODO: 今は2人しかいない前提だが3人以上も可能にする
  if (1 && freetalk.now) {
    switch (freetalk.k) {
      // 自分が喋り終わっていて、新規メッセージが届いていたら、ChatGPTに送信する
      case 0:
        if (tts.isNowPlayable()) {  // 自分が喋り終わったか？
          if (web.notice.newMessage || web.notice.newExmessage) {  // 新規メッセージが届いているか？（相手 or Web）
            resMessage = gpt.requestChat(true);  // 会話履歴をChatGPTに送信して、返答を履歴に追加する
            freetalk.tm = millis() + 500;
            freetalk.k++;
            web.notice.newMessage = false;
            web.notice.newExmessage = false;
            sp("free-1");
          }
        }
        break;
      // 相手が喋り終わるのを待つ
      case 1:
        if (freetalk.tm < millis()) {
          FriendStatus fstat;
          if (! singleMode) {
            fstat = web.checkFriendStatus(friendCharacterNo);  // 相手の状態を取得
          }
          if ((fstat.success && fstat.playlable) || !fstat.success || singleMode) {  // 仮：応答がない場合は抜ける
            freetalk.tm = 0;
            freetalk.k++;
            sp("free-2 fin");
          } else {
            sp("free-2 wait");
            freetalk.tm = millis() + 500;
          }
        }
        break;
      // 相手が喋り終わったら喋る
      case 2:
        sp("free-3");
        if (web.notice.newExmessageText != "") {  // Webからメッセージが届いたら相手にも伝える
          sp("free-3a");
          if (! singleMode) {
            bool noop = (resMessage != "");  // 続けて送信するメッセージがあるなら相手は応答保留の指示
            web.sendTalk(web.notice.newExmessageText, -1, friendCharacterNo, noop); // 相手にも内容を送信する（送信元はsystem）
          }
          web.notice.newExmessageText = "";
        }
        if (resMessage != "") {
          sp("free-3b");
          if (! singleMode) {
            web.sendTalk(resMessage, characterNo, friendCharacterNo, false); // 相手に喋る内容を送信する（送信元は自分）
          }
          tts.speak(resMessage);  // 喋る
        }
        freetalk.tm = 0;
        freetalk.k = 0;
        break;
    }
  }

  // デバッグ：音声レベル
  static unsigned long tmdbg = 0;
  if (tmdbg < millis()) {
    //sp("audioLevel="+String(tts.getLevel()));
    tmdbg = millis() + 100;
  }

  // 自動リップシンク・音声レベルで判定（母音aとoのみ）
  if (false && !tts.isNowPlayable()) {
    if (lipsync.tm < millis()) {
      int audioLevel = tts.getLevel();  // 現在再生中の音声レベルを取得
      static Vowel vowel = Vowel::n;
      unsigned long liptm;
      if (audioLevel < 100) {
        vowel = Vowel::n;
        liptm = 100;
        //sp("  LIP null");
      } else if (audioLevel < 800) {
        vowel = Vowel::o;
        liptm = 150;
        //sp("  LIP O");
      } else {
        vowel = Vowel::a;
        liptm = 180;
        //sp("  LIP A");
      }
      // avatar.setLipsyncVowel(static_cast<Vowel>(lipsync.k++ % 6));
      // lipsync.tm = millis() + 50 + random(50, 100);  // 次の変更タイミング
      avatar.setLipsyncVowel(vowel, liptm * 0.7);  // アバターを母音に合った口にする
      lipsync.tm = millis() + liptm;  // 次の変更タイミング
    }
  }

  // 自動リップシンク・母音データで判定（VOICEVOX REST APIのみ）
  if (true && !tts.isNowPlayable()) {
    if (lipsync.tm < millis()) {
      VowelData vdata = tts.getVowel();  // 現在発話中の母音情報を取得
      Vowel vowel = static_cast<Vowel>(vdata.vowel);
      if (vowel == Vowel::null) vowel = Vowel::n;
      //sp("  LIP "+String(vowel)+" ["+String(vdata.timeline)+"]");
      if (vdata.timeline == 0) vdata.timeline = 100;
      avatar.setLipsyncVowel(vowel, vdata.timeline * 0.7);  // アバターを母音に合った口にする
      lipsync.tm = millis() + vdata.timeline; // 次の変更タイミング
    }
  }

  // タッチモードが一定時間続いたら自由モードに戻す、それ以外はボヨンボヨンする
  const float boyonXs[] = { 1.0, 0.99, 0.98, 0.99 };
  const float boyonYs[] = { 1.0, 1.01, 1.02, 1.01 };
  if (stat == Mode::Touch) {
    if (mTouch.tm != 0 && mTouch.tm < millis()) {
      sp("Back to Free mode");
      mTouch.tm = 0;
      nextstat = Mode::Free;
      freeservo.now = true;
    } else if (tts.isNowPlayable()) { // TODO: ボヨンボヨンの周期を計算で求めたい（現状はただのループ）
      kp = mTouch.k++ % 4;
      // avatar.setEnpandCanvas(0, mTouch.k++%2*3, 0, 0);  // キャンバス内の表示位置変更
      avatar.scaleBodyCanvasY = boyonXs[kp];
      avatar.scaleBodyCanvasX = boyonYs[kp];
      refreshAvatar = true;
    }
  }

  // アバター全体の再描画を行う
  if (refreshAvatar) {
    avatar.drawAvatar();
  }

  if (nextstat != Mode::None) stat = nextstat;
  delay(5);
}

// ====================================================================================


/*
void loop_old1() {
  // ずんだもんを歩かせる
  if (true) {
    delay(2000);
    // 右から左へ
    avatar.changeParts("mouth", 1);   // 口
    int x, y, speed=5;
    avatar.mirrorImage = false;      // 左右反転
    for (x = 280; x>-120; x-=speed) {
      avatar.changeDrawPosition(x, 0);   // ディスプレイ上の出力位置
      //avatar.setEnpandCanvas(0, k%2*3, 0, 0);  // キャンバス内の表示位置変更
      avatar.scaleBodyCanvasY = 1.0 + k%2 * 0.02;
      avatar.drawAvatar(); // アバター全体表示
      M5.Lcd.fillRect(x+240,0, speed,M5.Lcd.width(), TFT_WHITE);
      //delay(100);
      k++;
    }
    // 左から右へ
    avatar.mirrorImage = true;      // 左右反転
    for (x = -120; x<280; x+=speed) {
      avatar.changeDrawPosition(x, 0);   // ディスプレイ上の出力位置
      avatar.scaleBodyCanvasY = 1.0 + k%2 * 0.02;
      //avatar.setEnpandCanvas(0, k%2*3, 0, 0);  // キャンバス内の表示位置変更
      avatar.drawAvatar(); // アバター全体表示
      M5.Lcd.fillRect(x-speed,0, speed,M5.Lcd.width(), TFT_WHITE);
      //delay(100);
      k++;
    }
    // 下から上へ
    M5.Lcd.fillScreen(TFT_WHITE);
    avatar.scaleBodyCanvasY = 1.0;
    avatar.setEnpandCanvas(0, 0, 0, 0);  // キャンバス内の表示位置変更
    avatar.mirrorImage = false;      // 左右反転
    avatar.changeParts("eyebrow", 1); // 眉毛
    avatar.changeParts("rhand", 1);   // 右腕
    avatar.changeParts("lhand", 1);   // 左腕
    for (y = 220; y>=0; y-=4) {
      avatar.changeDrawPosition(40, y);   // ディスプレイ上の出力位置
      avatar.drawAvatar(); // アバター全体表示
      //delay(100);
      k++;
    }
    // ドヤ
    delay(500);
    avatar.scaleBodyCanvasY = 1.0;
    avatar.changeParts("rhand", 3);   // 右腕
    avatar.changeParts("mouth", 3);   // 口
    avatar.drawAvatar(); // アバター全体表示
    delay(1500);
    // 戻し
    M5.Lcd.fillScreen(TFT_WHITE);
    avatar.changeParts("eyebrow", 0); // 眉毛
    avatar.changeParts("rhand", 0);   // 右腕
    avatar.changeParts("lhand", 0);   // 左腕
    avatar.changeParts("mouth", 0);   // 口
    k = 0;
  }

  // ぼよんぼよん
  unsigned long nexttime = 0;
  float scx[] = { 1.0, 0.99, 0.98, 0.99 };
  float scy[] = { 1.0, 1.01, 1.02, 1.01 };
  for (;;) {
    for (int i=0; i<4; i++) {
      while (millis() < nexttime) delay(1);
      nexttime = millis() + 150;
      avatar.scaleBodyCanvasY = scx[i];
      avatar.scaleBodyCanvasX = scy[i];
      avatar.drawAvatarAwait();
      //avatar.drawAvatar();
    }
  }

  // アバターの表示
  //avatar.setBackgroud(&display, 60,0, pose, zbgimgData[0]);
  delay(999999);
  //delay(1);
}
*/

// 空きメモリ確認
void debug_free_memory(String str) {
  sp("## "+str);
  spf("heap_caps_get_free_size(MALLOC_CAP_DMA):%d\n", heap_caps_get_free_size(MALLOC_CAP_DMA) );
  spf("heap_caps_get_largest_free_block(MALLOC_CAP_DMA):%d\n", heap_caps_get_largest_free_block(MALLOC_CAP_DMA) );
  spf("heap_caps_get_free_size(MALLOC_CAP_SPIRAM):%d\n", heap_caps_get_free_size(MALLOC_CAP_SPIRAM) );
  spf("heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM):%d\n\n", heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM) );
}
