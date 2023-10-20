#if !defined(BBDCMOTOR_H)
#define BBDCMOTOR_H

#include <Arduino.h>
#include <BBError.h>
#if defined(ARDUINO_ARCH_SAMD)
#include <Encoder.h>
#endif

namespace bb {

class DCMotor {
public:
  typedef enum {
    DCM_BRAKE,
    DCM_IDLE,
    DCM_FORWARD,
    DCM_BACKWARD
  } Direction;

  static const uint8_t PIN_OFF = 255;

  typedef enum {
    SCHEME_A_B_PWM,
    SCHEME_PWM_A_PWM_B
  } Scheme;

  // Use to initialize a motor that has A/B/PWM control scheme
  DCMotor(uint8_t pin_a, uint8_t pin_b, uint8_t pin_pwm, uint8_t pin_en = PIN_OFF);
  // Use to initialize a motor that has PWM_A/PWM_B control scheme
  DCMotor(uint8_t pin_pwm_a, uint8_t pin_pwm_b);

  virtual bool begin();

  virtual void setDirectionAndSpeed(Direction dir, uint8_t speed);
  virtual Direction direction() { return dir_; }
  virtual uint8_t speed() { return speed_; }
  virtual void setEnabled(bool en);
  virtual bool isEnabled() {
    return en_;
  }

protected:
  uint8_t pin_a_, pin_b_, pin_pwm_, pin_en_;
  Direction dir_;
  uint8_t speed_;
  bool en_;
  Scheme scheme_;
};

#if defined(ARDUINO_ARCH_SAMD)

class EncoderMotor: public DCMotor {
public:
  enum ControlMode {
    CONTROL_PWM,
    CONTROL_SPEED,
    CONTROL_POSITION
  };

  enum Unit {
    UNIT_MILLIMETERS,
    UNIT_TICKS
  };

  struct __attribute__ ((packed)) DriveControlState {
    ErrorState errorState;
    ControlMode controlMode;
    float presentPWM, presentSpeed, presentPos;
    float goal, err, errI, errD, control; 
  };

  EncoderMotor(uint8_t pin_a, uint8_t pin_b, uint8_t pin_pwm, uint8_t pin_en, uint8_t pin_enc_a, uint8_t pin_enc_b);
  EncoderMotor(uint8_t pin_pwm_a, uint8_t pin_pwm_b, uint8_t pin_enc_a, uint8_t pin_enc_b);

  void setReverse(bool reverse);

  void setMillimetersPerTick(float mmPT);
  void setMaxSpeed(float maxSpeed, Unit unit = UNIT_MILLIMETERS);
  void setAcceleration(float accel, Unit unit = UNIT_MILLIMETERS);

  // If mode is CONTROL_PWM, unit is disregarded.
  void setGoal(float goal, ControlMode mode, Unit unit = UNIT_MILLIMETERS); 
  float getGoal(Unit unit = UNIT_MILLIMETERS);
  ControlMode getControlMode();

  void setSpeedControlParameters(float kp, float ki, float kd);
  void getSpeedControlParameters(float &kp, float &ki, float &kd) { kp = kpSpeed_; ki = kiSpeed_; kd = kdSpeed_; }
  void getSpeedControlState(float& err, float& errI, float& errD, float& control) { err = errSpeedL_; errI = errSpeedI_; errD = errSpeedD_; control = controlSpeed_; }
  void setPosControlParameters(float kp, float ki, float kd);
  void getPosControlParameters(float &kp, float &ki, float &kd) { kp = kpPos_; ki = kiPos_; kd = kdPos_; }
  float getPresentPWM() { return presentPWM_; }
  float getPresentSpeed(Unit unit = UNIT_MILLIMETERS);
  float getPresentPosition(Unit unit = UNIT_MILLIMETERS);

  DriveControlState getDriveControlState();

  void update();

  long getLastCycleTicks() { return lastCycleTicks_; }

protected:
  void pwmControlUpdate(float dt);
  void speedControlUpdate(float dt);
  void positionControlUpdate(float dt);

  Encoder enc_;
  float mmPT_;
  long lastCycleTicks_;
  unsigned long lastCycleUS_;
  float maxSpeed_;

  ControlMode mode_;

  float accel_;

  float goal_;
  float presentPWM_;
  float presentSpeed_; // internally always in encoder ticks per second
  long presentPos_;    // internally always in encoder ticks

  float kpSpeed_, kiSpeed_, kdSpeed_;
  float errSpeedL_, errSpeedI_, errSpeedD_, controlSpeed_;
  float kpPos_, kiPos_, kdPos_;
  float errPosL_, errPosI_, errPosD_, controlPos_;

  bool reverse_;
};

#endif // ARDUINO_ARCH_SAMD


}; // namespace bb

#endif  // !defined(BBDCMOTOR_H)
