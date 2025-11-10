#include <Arduino.h>
#include <deque>
#include <SD.h>
#include <SPIFFS.h>
#include "mod/ModManager.h"
#include "MyStackChanMod.h"
#include <Avatar.h>
#include "Robot.h"
#include "llm/ChatGPT/ChatGPT.h"
#include "llm/ChatGPT/FunctionCall.h"
#include "driver/PlayMP3.h"
#include "driver/WakeWord.h"
#include "driver/ModuleLLM.h"
#include <WiFiClientSecure.h>
#include "MailClient.h"
#include "Scheduler.h"
#include "MySchedule.h"
#include "SDUtil.h"
#if defined( ENABLE_CAMERA )
#include "driver/Camera.h"
#endif
#include "driver/AudioWhisper.h"       //speechToText
#include "stt/Whisper.h"               //speechToText
#include "driver/Audio.h"              //speechToText
#include "stt/CloudSpeechClient.h"     //speechToText
#include "rootCA/rootCACertificate.h"  //speechToText
#include "rootCA/rootCAgoogle.h"       //speechToText
#include "driver/Audio.h"              //speechToText

using namespace m5avatar;

#if defined(ENABLE_WAKEWORD)
extern bool wakeword_is_enable;
#endif

/// 外部参照 ///
extern Avatar avatar;
extern bool servo_home;
extern bool servo_controled_by_me;
//extern bool wakeword_is_enable;
extern void sw_tone();
extern void alarm_tone();
//extern void report_batt_level();
///////////////

static void report_batt_level(){
  char buff[100];
  int level = M5.Power.getBatteryLevel();
#if defined(ENABLE_WAKEWORD)
  mode = 0;
#endif
  if(M5.Power.isCharging())
    sprintf(buff,"充電中、バッテリーのレベルは%d％です。",level);
  else
    sprintf(buff,"バッテリーのレベルは%d％です。",level);
  avatar.setExpression(Expression::Happy);
#if defined(ENABLE_WAKEWORD)
  mode = 0; 
#endif
  robot->speech(String(buff));
  delay(1000);
  avatar.setExpression(Expression::Neutral);
}

static void nodOnce() {
    bool prev_servo_home = servo_home;  // 現在のservo_homeの状態を保存
    int currentY = robot->m_config.getServoInfo(AXIS_Y)->start_degree;  // 現在のY軸の初期位置を取得

    servo_controled_by_me = true;
    servo_home = false;  // サーボの自動制御を無効化

    // 下を向く（500msで移動）- 現在位置から15度下向き
    robot->servo->moveY(currentY - 10, 500);
    delay(500);  // 移動完了を待つ
    
    // 元の位置に戻る（500msで移動）
    robot->servo->moveY(currentY, 500);
    delay(500);  // 移動完了を待つ
    
    servo_controled_by_me = false;
    servo_home = prev_servo_home;  // servo_homeの状態を元に戻す
}

static void MyResponse(String user_text){
  if(user_text.indexOf("こんにち") != -1){
    avatar.setExpression(Expression::Happy);
    robot->speech("こんにちは。");

    delay(1000);
    nodOnce();
    delay(1000);

    robot->speech("ごきげんいかがですか？");
    avatar.setExpression(Expression::Neutral);
    return;
  } else if(user_text.indexOf("元気") != -1) {
    avatar.setExpression(Expression::Happy);
    robot->speech("おかげさまで元気です。ありがとうございます。");
    avatar.setExpression(Expression::Neutral);
    return;
  } else if(user_text.indexOf("名前") != -1){
    avatar.setExpression(Expression::Happy);
    robot->speech("私の名前はスタックちゃんです。よろしくお願いします。");
    avatar.setExpression(Expression::Neutral);
    return;
  } 
}

static void STT_MyChat(const char *base64_buf = NULL) {
  bool prev_servo_home = servo_home;
#ifdef USE_SERVO
  servo_home = true;
#endif

  avatar.setExpression(Expression::Happy);
  avatar.setSpeechText("御用でしょうか？");

  String ret = robot->listen();
  avatar.setSpeechText("");

#ifdef USE_SERVO
  //servo_home = prev_servo_home;
  servo_home = false;
#endif
  Serial.println("音声認識終了");
  Serial.println("音声認識結果");
  if(ret != "") {
    Serial.println(ret);
    MyResponse(ret);
    avatar.setSpeechText("");
    avatar.setExpression(Expression::Neutral);
    servo_home = true;
#if defined(ENABLE_WAKEWORD)
    mode = 0;
#endif
  } else {
    Serial.println("音声認識失敗");
    avatar.setExpression(Expression::Sad);
    avatar.setSpeechText("聞き取れませんでした");
    delay(2000);
    avatar.setSpeechText("");
    avatar.setExpression(Expression::Neutral);
    servo_home = true;
  } 
}

MyStackChanMod::MyStackChanMod(bool _isOffline)
  : isOffline{_isOffline}
{
  box_servo.setupBox(80, 120, 80, 80);
#if defined(ENABLE_CAMERA)
  box_stt.setupBox(107, 0, M5.Display.width()-107, 80);
  box_subWindow.setupBox(0, 0, 107, 80);
#else
  box_stt.setupBox(0, 0, M5.Display.width(), 60);
#endif
  box_BtnA.setupBox(0, 100, 40, 60);
  box_BtnC.setupBox(280, 100, 40, 60);

  //SDカードのMP3ファイル（アラーム用）をSPIFFSにコピーする（SDカードだと音が途切れ途切れになるため）。
  //すでにSPIFFSにファイルがあればコピーはしない。強制的にコピー（上書き）したい場合は第2引数をtrueにする。
  //String fname = String(APP_DATA_PATH) + String(FNAME_ALARM_MP3);
  //copySDFileToSPIFFS(fname.c_str(), false);

  if(robot->m_config.getExConfig().wakeword.type == WAKEWORD_TYPE_MODULE_LLM_KWS){
#if defined(USE_LLM_MODULE)
    // Nothing to initialize here
#endif
  }
  else{
#if defined(ENABLE_WAKEWORD)
    wakeword_init();
#endif
  }

}


void MyStackChanMod::init(void)
{
  avatar.setSpeechText("My Stack-chan");
#if defined(ENABLE_CAMERA)
  if(isSubWindowON){
    avatar.set_isSubWindowEnable(true);
  }
#endif
}

void MyStackChanMod::pause(void)
{
#if defined(ENABLE_CAMERA)
  if(isSubWindowON){
    avatar.set_isSubWindowEnable(false);
  }
#endif
}


void MyStackChanMod::update(int page_no)
{

}

void MyStackChanMod::btnA_pressed(void)
{
#if defined(ENABLE_WAKEWORD)
  if(mode >= 0){
    sw_tone();
    if(mode == 0){
      avatar.setSpeechText("ウェイクワード有効");
      mode = 1;
      wakeword_is_enable = true;
    } else {
      avatar.setSpeechText("ウェイクワード無効");
      mode = 0;
      wakeword_is_enable = false;
    }
    delay(1000);
    avatar.setSpeechText("");
  }
#endif
}


void MyStackChanMod::btnB_longPressed(void)
{
#if defined(ENABLE_WAKEWORD)
  M5.Mic.end();
  M5.Speaker.tone(1000, 100);
  delay(500);
  M5.Speaker.tone(600, 100);
  delay(1000);
  M5.Speaker.end();
  M5.Mic.begin();
  wakeword_is_enable = false; //wakeword 無効
  mode = -1;
#ifdef USE_SERVO
    servo_home = true;
    delay(500);
#endif
  avatar.setSpeechText("ウェイクワード登録開始");
#endif
}

void MyStackChanMod::btnC_pressed(void)
{
  sw_tone();
  report_batt_level();
  nodOnce();
}

void MyStackChanMod::btnB_pressed(void)
{
  sw_tone();
  nodOnce();
}

void MyStackChanMod::display_touched(int16_t x, int16_t y)
{
  if (box_stt.contain(x, y))
  {
    sw_tone();
    STT_MyChat();
  }
#ifdef USE_SERVO
  if (box_servo.contain(x, y))
  {
    sw_tone();
  }
#endif
  if (box_BtnA.contain(x, y))
  {
#if defined(ENABLE_CAMERA)
    isSilentMode = !isSilentMode;
    if(isSilentMode){
      avatar.setSpeechText("サイレントモード");
    }
    else{
      avatar.setSpeechText("サイレントモード解除");
    }
    delay(2000);
    avatar.setSpeechText("");
#else
    sw_tone();
#endif
  }
  if (box_BtnC.contain(x, y))
  {
    //sw_tone();
  }
}


void MyStackChanMod::idle(void)
{

  //Wakeword
  if(robot->m_config.getExConfig().wakeword.type == WAKEWORD_TYPE_MODULE_LLM_KWS){
#if defined(USE_LLM_MODULE)
    if( check_kws_wakeup() ){
      sw_tone();
      STT_MyChat();
    }
#else
    Serial.println("ModuleLLM is not enabled. Please define USE_LLM_MODULE.");
    delay(1000);
#endif
  }
  else{
#if defined(ENABLE_WAKEWORD)
    if (mode == 0) { /* return; */ }
    else if (mode < 0) {
      int idx = wakeword_regist();
      if(idx >= 0){
        String text = String("ウェイクワード#") + String(idx) + String("登録終了");
        avatar.setSpeechText(text.c_str());
        delay(1000);
        avatar.setSpeechText("");
        //mode = 0;
        //wakeword_is_enable = false;
        mode = 1;
        wakeword_is_enable = true;

      }
    }
    else if (mode > 0 && wakeword_is_enable) {
      int idx = wakeword_compare();
      if( idx >= 0){
        Serial.println("wakeword_compare OK!");
        String text = String("ウェイクワード#") + String(idx);
        avatar.setSpeechText(text.c_str());
        sw_tone();
        STT_MyChat();
      }
    }

#if defined(ARDUINO_M5STACK_CORES3)
#if defined(USE_EXTENSION_FUNCTIONS)
    // Function Callからの要求でウェイクワード有効化
    if (wakeword_enable_required)
    {
      wakeword_enable_required = false;
      btnA_pressed();
    }

    // Function Callからの要求でウェイクワード登録
    if(register_wakeword_required)
    {
      register_wakeword_required = false;
      
    }
#endif  //defined(USE_EXTENSION_FUNCTIONS)
#endif  //defined(ARDUINO_M5STACK_CORES3)
#endif  //ENABLE_WAKEWORD
  }

  /// Alarm ///
  if(xAlarmTimer != NULL){
    TickType_t xRemainingTime;

    /* Query the period of the timer that expires. */
    xRemainingTime = xTimerGetExpiryTime( xAlarmTimer ) - xTaskGetTickCount();
    avatarText = "残り" + String(xRemainingTime / 1000) + "秒";
    avatar.setSpeechText(avatarText.c_str());
  }

  if (alarmTimerCallbacked) {
    alarmTimerCallbacked = false;
    if(!SD.begin(GPIO_NUM_4, SPI, 25000000)) {
    //if(!SPIFFS.begin(true)){
      Serial.println("Failed to mount SD card. Use alarm tone.");
      alarm_tone();
    }
    else{
      String fname = String(APP_DATA_PATH) + String(FNAME_ALARM_MP3);
      bool result = playMP3SD(fname.c_str());
      if(!result){
        alarm_tone();
      }
    }
  }

  //スケジューラ処理
  if(!isOffline){
    run_schedule();
  }

}


