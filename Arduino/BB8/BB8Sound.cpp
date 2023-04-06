#include "BB8Sound.h"

BB8Sound BB8Sound::sound;

BB8Sound::BB8Sound(): dfp_(NULL) {
}

BB8Sound::~BB8Sound() {
  if(dfp_ != NULL) {
    DFRobotDFPlayerMini *temp = dfp_; // eliminate race conditions
    dfp_ = NULL;
    delete temp;
  }
}

bool BB8Sound::begin(Uart *ser) {
  if(ser == NULL) {
    Serial.println("Serial is NULL!"); 
    return false;
  } 
  #if 0
  if(dfp_ != NULL) {
    Serial.println("Sound already setup");
    return false;
  } 
  #endif 

  dfp_ = new DFRobotDFPlayerMini();
  Serial.print("Setting up sound... ");
  ser->begin(9600);
  if(dfp_->begin(*ser)) {
    dfp_->volume(100);
    dfp_->play(1);
    Serial.println("success.");
    return true;
  } else {
    Serial.print("error code ");
    Serial.print(dfp_->readType());
    Serial.println("... failed!");
    delete dfp_;
    dfp_ = NULL;
    return false;
  }
}

bool BB8Sound::play(int filenumber) {
  if(NULL == dfp_) return false;
  dfp_->play(filenumber);
  return true;
}

bool BB8Sound::setVolume(uint8_t vol) {
  if(NULL == dfp_) return false;
  dfp_->volume(vol);
  return true;
}
