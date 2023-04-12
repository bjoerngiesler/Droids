#include <math.h>

#include "BB8Controllers.h"
#include "BB8IMU.h"
#include "BB8Servos.h"

BB8BodyIMUControlInput::BB8BodyIMUControlInput(BB8BodyIMUControlInput::ProbeType pt) {
  pt_ = pt;
}

float BB8BodyIMUControlInput::getValue() {
  float r, p, h;
  if(BB8BodyIMU::imu.getFilteredRPH(r, p, h) == false) return 0.0f;
  
  switch(pt_) {
  case IMU_PROBE_ROLL:
    return r;
    break;
  case IMU_PROBE_PITCH:
    return p;
    break;
  case IMU_PROBE_HEADING:
    return h;
    break;
  }

  return 0.0f;
}

BB8ServoControlOutput::BB8ServoControlOutput(uint8_t sn, float offset) {
  sn_ = sn;
  offset_ = offset;
}

bool BB8ServoControlOutput::available() {
  //return BB8Servos::servos.available();
  return false;
}

bool BB8ServoControlOutput::setValue(float value) {
  if(!isEnabled()) return false;

  value += BB8Servos::servos.getPresentPosition(sn_);
  return BB8Servos::servos.setPosition(sn_, value);
}

bool BB8ServoControlOutput::enable(bool onoff) {
  if(onoff && !isEnabled()) {
    if(BB8Servos::servos.switchTorque(sn_, true) == false) {
      Serial.println("Couldn't switch torque on");
      return false;
    } else return true;
  } else if(!onoff && isEnabled()) {
    return BB8Servos::servos.switchTorque(sn_, false);
  } else return true;
}

bool BB8ServoControlOutput::isEnabled() {
  return BB8Servos::servos.isTorqueOn(sn_);
}

BB8PIDController BB8PIDController::rollController;
BB8BodyIMUControlInput rollControlInput(BB8BodyIMUControlInput::IMU_PROBE_ROLL);
BB8ServoControlOutput rollControlOutput(4, 180.0f);

BB8PIDController::BB8PIDController() {
  input_ = NULL;
  output_ = NULL;
  kp_ = ki_ = kd_ = 0.0f;
  setpoint_ = 0.0f;
  deadband_ = 0.0f;
  i_ = 0.0f;
  enabled_ = false;
}

bool BB8PIDController::begin() {
  // FIXME
  input_ = &rollControlInput;
  output_ = &rollControlOutput;
  setpoint_ = input_->getValue();
  enabled_ = false;
  return true;
}

bool BB8PIDController::available() {
  if(NULL == output_) return false;
  return output_->available();
}

void BB8PIDController::setSetpoint(float sp) {
  setpoint_ = sp;
}

bool BB8PIDController::step(bool outputForPlot) {
  if(NULL == input_ || NULL == output_ || enabled_ == false) {
    return false;
  }
  float val = input_->getValue();
  float e = (setpoint_ - val);

  i_ += e;
  if(fabs(i_) > fabs(iabort_)) {
    Serial.print("Integral part "); Serial.print(i_); Serial.print(" > IAbort "); Serial.print(iabort_); Serial.println(". Aborting.");
    enable(false);
    return true;
  }

#if 0
  float tmp_i_ = i_;
  if(i_ < -imax_) tmp_i_ = -imax_;
  else if(i_ > imax_) tmp_i_ = imax_;
#else
  if(i_ < -imax_) i_ = -imax_;
  else if(i_ > imax_) i_ = imax_;
  float tmp_i_ = i_;
#endif

  float d = last_e_ == 0.0f ? 0.0f : (last_e_ - e);

  float cp = e * kp_ + tmp_i_ * ki_ + d * kd_;
  
  if(outputForPlot) {
    Serial.print("SP:"); Serial.print(setpoint_);
    Serial.print(", E:"); Serial.print(e);
    Serial.print(", I:"); Serial.print(i_);
    Serial.print(", D:"); Serial.print(d);
    Serial.print(", CP:"); Serial.println(cp);
  }

  output_->setValue(cp);

  last_e_ = e;
  return true;
}

bool BB8PIDController::enable(bool onoff) {
  if(NULL == output_) { Serial.println("No output!"); return false; }
  if(onoff) {
    // FIXME

    i_ = 0.0f;
    if(!output_->enable(true)) return false;
    enabled_ = true;
  } else {
    if(!output_->enable(false)) return false;
    enabled_ = false;
  }
  return true;
}

bool BB8PIDController::isEnabled() {
  if(NULL == output_) return false;
  return output_->isEnabled();
}