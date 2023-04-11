#include "BB8.h"
#include "BB8Config.h"
#include "BB8Servos.h"
#include "BB8DCMotor.h"
#include "BB8StatusPixels.h"
#include "BB8IMU.h"
#include "BB8Sound.h"

BB8 BB8::bb8;

ServoLimits servolimits[] = {
  {0.0f, 360.0f, 0.0f, 60.0},
  {120.0f, 240.0f, 0.0f, 60.0},
  {120.0f, 240.0f, 0.0f, 60.0},
  {160.0f, 200.0f, 0.0f, 80.0}
};

//FIXME
BB8DCMotor driveMotor(P_DRIVE_A, P_DRIVE_B, P_DRIVE_PWM, P_DRIVE_EN);
BB8DCMotor yawMotor(P_YAW_A, P_YAW_B, P_YAW_PWM, P_YAW_EN);

BB8::BB8() {
    name_ = "bb8";
    description_ = "BB8 Main System";
    help_ = "BB8 Main System\r\n"\
"Available commands:\r\n"\
"        status                 Prints current status (axes, motors, commands, packet loss statistics, etc.)\r\n"\
"        running_status on|off  Continuously prints status\r\n"\
"        play_sound <num>       Play sound with given number\r\n"\
"        calibrate              Calibrate gyro\r\n"\
"        motor <num>            Set drive motor speed\r\n"\
"        kiosk_mode on|off      Disable all body movement, activate random dome movement";
    started_ = false;
    operationStatus_ = RES_SUBSYS_NOT_STARTED;

    parameters_.push_back({"drive_speed_factor", PARAMETER_FLOAT, "Multiplier for drive speed coming from the remote (0.0-2.0)"});
    parameters_.push_back({"turn_speed_factor", PARAMETER_FLOAT, "Multiplier for turn speed coming from the remote (0.0-2.0)"});
    parameters_.push_back({"body_roll_servo_min", PARAMETER_FLOAT, "Minimum angle for body roll servo (degrees, 180.0 is center)"});
    parameters_.push_back({"body_roll_servo_max", PARAMETER_FLOAT, "Maximum angle for body roll servo (degrees, 180.0 is center)"});
    parameters_.push_back({"body_roll_servo_offset", PARAMETER_FLOAT, "Offset for body roll servo (degrees, 180.0 is center, adapt to set at-rest bias)"});
    parameters_.push_back({"body_roll_servo_speed", PARAMETER_FLOAT, "Max motion speed for body roll servo (deg/s)"});
    parameters_.push_back({"body_roll_servo_invert", PARAMETER_UINT, "Invert body roll servo"});
    parameters_.push_back({"dome_pitch_servo_min", PARAMETER_FLOAT, "Minimum angle for dome pitch servo (degrees/2, 180.0 is center)"});
    parameters_.push_back({"dome_pitch_servo_max", PARAMETER_FLOAT, "Maximum angle for dome pitch servo (degrees/2, 180.0 is center)"});
    parameters_.push_back({"dome_pitch_servo_offset", PARAMETER_FLOAT, "Offset for dome pitch servo (degrees/2, 180.0 is center, adapt to set at-rest bias)"});
    parameters_.push_back({"dome_pitch_servo_speed", PARAMETER_FLOAT, "Max motion speed for dome pitch servo (deg/s)"});
    parameters_.push_back({"dome_pitch_servo_invert", PARAMETER_UINT, "Invert dome pitch servo"});
    parameters_.push_back({"dome_roll_servo_min", PARAMETER_FLOAT, "Minimum angle for dome roll servo (degrees/2, 180.0 is center)"});
    parameters_.push_back({"dome_roll_servo_max", PARAMETER_FLOAT, "Maximum angle for dome roll servo (degrees/2, 180.0 is center)"});
    parameters_.push_back({"dome_roll_servo_offset", PARAMETER_FLOAT, "Offset for dome roll servo (degrees/2, 180.0 is center, adapt to set at-rest bias)"});
    parameters_.push_back({"dome_roll_servo_speed", PARAMETER_FLOAT, "Max motion speed for dome roll servo (deg/s)"});
    parameters_.push_back({"dome_roll_servo_invert", PARAMETER_UINT, "Invert dome roll servo"});
    parameters_.push_back({"dome_heading_servo_min", PARAMETER_FLOAT, "Minimum angle for dome yaw servo (degrees, 180.0 is center)"});
    parameters_.push_back({"dome_heading_servo_max", PARAMETER_FLOAT, "Maximum angle for dome yaw servo (degrees, 180.0 is center)"});
    parameters_.push_back({"dome_heading_servo_offset", PARAMETER_FLOAT, "Offset for dome yaw servo (degrees, 180.0 is center, adapt to set at-rest bias)"});
    parameters_.push_back({"dome_heading_servo_speed", PARAMETER_FLOAT, "Max motion speed for dome yaw servo (deg/s)"});
    parameters_.push_back({"dome_heading_servo_invert", PARAMETER_UINT, "Invert dome heading servo"});
  }

Result BB8::initialize() {
  paramsHandle_ = ConfigStorage::storage.reserveBlock(sizeof(params_));
  if(ConfigStorage::storage.blockIsValid(paramsHandle_)) {
    ConfigStorage::storage.readBlock(paramsHandle_, (uint8_t*)&params_);

    servolimits[BODY_ROLL_SERVO-1].min = params_.body_roll_servo_min;
    servolimits[BODY_ROLL_SERVO-1].max = params_.body_roll_servo_max;
    servolimits[BODY_ROLL_SERVO-1].offset = params_.body_roll_servo_offset;
    servolimits[BODY_ROLL_SERVO-1].speed = params_.body_roll_servo_speed;
    
    servolimits[BODY_ROLL_SERVO-1].min = params_.dome_pitch_servo_min;
    servolimits[BODY_ROLL_SERVO-1].max = params_.dome_pitch_servo_max;
    servolimits[BODY_ROLL_SERVO-1].offset = params_.dome_pitch_servo_offset;
    servolimits[BODY_ROLL_SERVO-1].speed = params_.dome_pitch_servo_speed;
    
    servolimits[DOME_ROLL_SERVO-1].min = params_.dome_roll_servo_min;
    servolimits[DOME_ROLL_SERVO-1].max = params_.dome_roll_servo_max;
    servolimits[DOME_ROLL_SERVO-1].offset = params_.dome_roll_servo_offset;
    servolimits[DOME_ROLL_SERVO-1].speed = params_.dome_roll_servo_speed;

    servolimits[DOME_HEADING_SERVO-1].min = params_.dome_heading_servo_min;
    servolimits[DOME_HEADING_SERVO-1].max = params_.dome_heading_servo_max;
    servolimits[DOME_HEADING_SERVO-1].offset = params_.dome_heading_servo_offset;
    servolimits[DOME_HEADING_SERVO-1].speed = params_.dome_heading_servo_speed;
  } else {
    params_.drive_speed_factor = 0.5;
    params_.turn_speed_factor = 1.0;
    
    params_.body_roll_servo_min = servolimits[BODY_ROLL_SERVO-1].min;
    params_.body_roll_servo_max = servolimits[BODY_ROLL_SERVO-1].max;
    params_.body_roll_servo_offset = servolimits[BODY_ROLL_SERVO-1].offset;
    params_.body_roll_servo_speed = servolimits[BODY_ROLL_SERVO-1].speed;
    params_.body_roll_servo_invert = true;
    
    params_.dome_pitch_servo_min = servolimits[DOME_PITCH_SERVO-1].min;
    params_.dome_pitch_servo_max = servolimits[DOME_PITCH_SERVO-1].max;
    params_.dome_pitch_servo_offset = servolimits[DOME_PITCH_SERVO-1].offset;
    params_.dome_pitch_servo_speed = servolimits[DOME_PITCH_SERVO-1].speed;
    params_.dome_pitch_servo_invert = false;
    
    params_.dome_roll_servo_min = servolimits[DOME_ROLL_SERVO-1].min;
    params_.dome_roll_servo_max = servolimits[DOME_ROLL_SERVO-1].max;
    params_.dome_roll_servo_offset = servolimits[DOME_ROLL_SERVO-1].offset;
    params_.dome_roll_servo_speed = servolimits[DOME_ROLL_SERVO-1].speed;
    params_.dome_roll_servo_invert = false;

    params_.dome_heading_servo_min = servolimits[DOME_HEADING_SERVO-1].min;
    params_.dome_heading_servo_max = servolimits[DOME_HEADING_SERVO-1].max;
    params_.dome_heading_servo_offset = servolimits[DOME_HEADING_SERVO-1].offset;
    params_.dome_heading_servo_speed = servolimits[DOME_HEADING_SERVO-1].speed;
    params_.dome_heading_servo_invert = false;
  }

  return Subsystem::initialize();
}

Result BB8::start(ConsoleStream *stream) {
  started_ = true;
  runningStatus_ = false;
  kioskMode_ = false;
  operationStatus_ = RES_OK;
  bb::XBee::xbee.addPacketReceiver(this);
  packetsReceived_ = packetsMissed_ = 0;
  memset((uint8_t*)&lastPacket_, 0, sizeof(lastPacket_));
  packetTimeout_ = 0;

  if(!BB8Servos::servos.isStarted()) {
    if(BB8Servos::servos.start(stream) != RES_OK) {
      return RES_SUBSYS_HW_DEPENDENCY_MISSING;
    }

    if(BB8Servos::servos.moveAllServosToOrigin() != RES_OK) {
      return RES_SUBSYS_HW_DEPENDENCY_MISSING;
    }
  }

  BB8BodyIMU::imu.begin();

  driveMotor.setEnabled(true);
  driveMotor.setDirectionAndSpeed(BB8DCMotor::DCM_BRAKE, 0);

  return RES_OK;
}
	
Result BB8::stop(ConsoleStream *stream) {
  started_ = false;
  operationStatus_ = RES_SUBSYS_NOT_STARTED;
  bb::XBee::xbee.removePacketReceiver(this);
  return RES_OK;
}

Result BB8::step() {
  if(packetTimeout_ > 0) BB8StatusPixels::statusPixels.setPixel(STATUSPIXEL_NETWORK, 0, 255, 0);
  else BB8StatusPixels::statusPixels.setPixel(STATUSPIXEL_NETWORK, 255, 0, 0);

  BB8BodyIMU::imu.step(false);

  if(runningStatus_) {
    printStatus();
    Console::console.printBroadcast("\r");
  }

  if(packetTimeout_ > 0) packetTimeout_--;

  if(kioskMode_) {
    if(kioskDelay_ < BBRUNLOOP_CYCLETIME) {
      if(random(0, 2) != 0) {
        BB8Servos::servos.setSpeed(DOME_HEADING_SERVO, (float)random(servolimits[DOME_HEADING_SERVO].speed/2, servolimits[DOME_HEADING_SERVO].speed));      
        BB8Servos::servos.setSetpoint(DOME_HEADING_SERVO, (float)random(servolimits[DOME_HEADING_SERVO].min, servolimits[DOME_HEADING_SERVO].max));      
      } else {
        Serial.println("No motion");
      }
      if(random(0, 2) != 0) {
        BB8Servos::servos.setSpeed(DOME_ROLL_SERVO, (float)random(servolimits[DOME_ROLL_SERVO].speed/2, servolimits[DOME_ROLL_SERVO].speed));      
        BB8Servos::servos.setSetpoint(DOME_ROLL_SERVO, (float)random(servolimits[DOME_ROLL_SERVO].min, servolimits[DOME_ROLL_SERVO].max));      
      }
      if(random(0, 2) != 0) {
        BB8Servos::servos.setSpeed(DOME_PITCH_SERVO, (float)random(servolimits[DOME_PITCH_SERVO].speed/2, servolimits[DOME_PITCH_SERVO].speed));      
        BB8Servos::servos.setSetpoint(DOME_PITCH_SERVO, (float)random(servolimits[DOME_PITCH_SERVO].min, servolimits[DOME_PITCH_SERVO].max));
      }
      kioskDelay_ = random(2000, 5000);
    } else kioskDelay_ -= BBRUNLOOP_CYCLETIME;
  }

  return RES_OK;
}

Result BB8::incomingPacket(const Packet& packet) {
  if(packet.source == PACKET_SOURCE_TEST_ONLY) { 
    Console::console.printlnBroadcast("Test packet received!"); 
    return RES_OK;
  }

  if(kioskMode_) return RES_SUBSYS_WRONG_MODE;

  if(packet.type != PACKET_TYPE_COMMAND) return RES_SUBSYS_PROTOCOL_ERROR;

  packetsReceived_++;
  if(packet.seqnum != (lastPacket_.seqnum+1)%MAX_SEQUENCE_NUMBER) packetsMissed_++; // FIXME not correct - should count based on seqnum
  lastPacket_ = packet;

  float r, p, h;
  BB8BodyIMU::imu.getFilteredRPH(r, p, h);
  if(packet.payload.cmd.button2) {
    //Console::console.printlnBroadcast("Updating servos");
    float axis2, axis3, axis4;
    if(params_.dome_roll_servo_invert)
      axis2 = 180.0 - ((packet.payload.cmd.getAxis(2)*30.0)*4/127.0 - 2*(BB8Servos::servos.getPresentPosition(BODY_ROLL_SERVO)-180.0) + 2*r);
    else 
      axis2 = 180.0 + ((packet.payload.cmd.getAxis(2)*30.0)*4/127.0 - 2*(BB8Servos::servos.getPresentPosition(BODY_ROLL_SERVO)-180.0) + 2*r);
    if(params_.dome_pitch_servo_invert)
      axis3 = 180.0 - ((packet.payload.cmd.getAxis(3)*30.0)*4/127.0 + 2*p);
    else 
      axis3 = 180.0 + ((packet.payload.cmd.getAxis(3)*30.0)*4/127.0 + 2*p);
    if(params_.dome_heading_servo_invert)
      axis4 = 180.0 - ((packet.payload.cmd.getAxis(4)*30.0)*4/127.0);
    else
      axis4 = 180.0 + ((packet.payload.cmd.getAxis(4)*30.0)*4/127.0);
      
    BB8Servos::servos.setSetpoint(DOME_ROLL_SERVO, axis2);
    BB8Servos::servos.setSetpoint(DOME_PITCH_SERVO, axis3);
    BB8Servos::servos.setSetpoint(DOME_HEADING_SERVO, axis4);
  } else {
    //BB8Servos::servos.setSetpoint(DOME_ROLL_SERVO, 180.0+2*r);
    if(params_.dome_roll_servo_invert)
      BB8Servos::servos.setSetpoint(DOME_ROLL_SERVO, 180.0 - (2*r - 2*(BB8Servos::servos.getPresentPosition(BODY_ROLL_SERVO)-180.0)));
    else
      BB8Servos::servos.setSetpoint(DOME_ROLL_SERVO, 180.0 + (2*r - 2*(BB8Servos::servos.getPresentPosition(BODY_ROLL_SERVO)-180.0)));

    if(params_.dome_pitch_servo_invert)
      BB8Servos::servos.setSetpoint(DOME_PITCH_SERVO, 180.0-2*p);
    else 
      BB8Servos::servos.setSetpoint(DOME_PITCH_SERVO, 180.0+2*p);
    BB8Servos::servos.setSetpoint(DOME_HEADING_SERVO, 180.0);
  }

  if(packet.payload.cmd.button1) {
    uint8_t axis1 = packet.payload.cmd.getAxis(1);
    Serial.println(String("Drive: ") + axis1);
    if(axis1 < 0) driveMotor.setDirectionAndSpeed(BB8DCMotor::DCM_BACKWARD, -axis1 * params_.drive_speed_factor);
    else driveMotor.setDirectionAndSpeed(BB8DCMotor::DCM_FORWARD, axis1 * params_.drive_speed_factor);

    float axis0;
    if(params_.body_roll_servo_invert)
      axis0 = 180.0 - (packet.payload.cmd.getAxis(0)*20.0)/127.0;
    else
      axis0 = 180.0 + (packet.payload.cmd.getAxis(0)*20.0)/127.0;
    BB8Servos::servos.setSetpoint(BODY_ROLL_SERVO, axis0);      
  } else {
    driveMotor.setDirectionAndSpeed(BB8DCMotor::DCM_IDLE, 0);
    BB8Servos::servos.setSetpoint(BODY_ROLL_SERVO, 180.0);      
  }

  packetTimeout_ = 3;

  return RES_OK;
}

Result BB8::handleConsoleCommand(const std::vector<String>& words, ConsoleStream *stream) {
  if(words.size() == 0) return RES_CMD_UNKNOWN_COMMAND;
  if(words[0] == "status") {
    if(words.size() != 1) return RES_CMD_INVALID_ARGUMENT_COUNT;
    printStatus(stream);
    stream->println();
    return RES_OK;
  } 
  
  else if(words[0] == "running_status") {
    if(words.size() != 2) return RES_CMD_INVALID_ARGUMENT_COUNT;
    if(words[1] == "on" || words[1] == "true") {
      runningStatus_ = true;
      return RES_OK;
    } else if(words[1] == "off" || words[1] == "false") {
      runningStatus_ = false;
      return RES_OK;
    }
    return RES_CMD_INVALID_ARGUMENT;
  } 
  
  else if(words[0] == "play_sound") {
    if(words.size() != 2) return RES_CMD_INVALID_ARGUMENT_COUNT;
    int sound = words[1].toInt();
    if(BB8Sound::sound.play(sound) == false) return RES_CMD_FAILURE;
    return RES_OK;
  } 

  else if(words[0] == "calibrate") {
    BB8BodyIMU::imu.calibrateGyro(stream);
  }

  else if(words[0] == "motor") {
    if(words.size() != 2) return RES_CMD_INVALID_ARGUMENT_COUNT;
    int num = words[1].toInt();
    driveMotor.setEnabled(true);
    if(num == 0) 
      driveMotor.setDirectionAndSpeed(BB8DCMotor::DCM_IDLE, 0);
    else if(num > 0)
      driveMotor.setDirectionAndSpeed(BB8DCMotor::DCM_FORWARD, num);
    else if(num < 0)
      driveMotor.setDirectionAndSpeed(BB8DCMotor::DCM_BACKWARD, -num);
  }

  else if(words[0] == "kiosk_mode") {
    if(words.size() != 2) return RES_CMD_INVALID_ARGUMENT_COUNT;
    if(words[1] == "on") {
      kioskMode_ = true;
      kioskDelay_ = 5000;
      return RES_OK;
    } else if(words[1] == "off") {
      kioskMode_ = false;
      BB8Servos::servos.setSpeed(DOME_HEADING_SERVO, servolimits[DOME_HEADING_SERVO].speed);
      BB8Servos::servos.setSpeed(DOME_PITCH_SERVO, servolimits[DOME_PITCH_SERVO].speed);
      BB8Servos::servos.setSpeed(DOME_ROLL_SERVO, servolimits[DOME_ROLL_SERVO].speed);
      return RES_OK;
    } else return RES_CMD_INVALID_ARGUMENT;
  }

  return RES_CMD_UNKNOWN_COMMAND;
}

Result BB8::parameterValue(const String& name, String& value) {
  if(name == "drive_speed_factor") { value = String(params_.drive_speed_factor); return RES_OK; }
  if(name == "turn_speed_factor") { value = String(params_.turn_speed_factor); return RES_OK; }
  if(name == "body_roll_servo_min") { value = String(params_.body_roll_servo_min); return RES_OK; }
  if(name == "body_roll_servo_max") { value = String(params_.body_roll_servo_max); return RES_OK; }
  if(name == "body_roll_servo_offset") { value = String(params_.body_roll_servo_offset); return RES_OK; }
  if(name == "body_roll_servo_speed") { value = String(params_.body_roll_servo_speed); return RES_OK; }
  if(name == "body_roll_servo_invert") { value = String(params_.body_roll_servo_invert); return RES_OK; }
  if(name == "dome_pitch_servo_min") { value = String(params_.dome_pitch_servo_min); return RES_OK; }
  if(name == "dome_pitch_servo_max") { value = String(params_.dome_pitch_servo_max); return RES_OK; }
  if(name == "dome_pitch_servo_offset") { value = String(params_.dome_pitch_servo_offset); return RES_OK; }
  if(name == "dome_pitch_servo_speed") { value = String(params_.dome_pitch_servo_speed); return RES_OK; }
  if(name == "dome_pitch_servo_invert") { value = String(params_.dome_pitch_servo_invert); return RES_OK; }
  if(name == "dome_roll_servo_min") { value = String(params_.dome_roll_servo_min); return RES_OK; }
  if(name == "dome_roll_servo_max") { value = String(params_.dome_roll_servo_max); return RES_OK; }
  if(name == "dome_roll_servo_offset") { value = String(params_.dome_roll_servo_offset); return RES_OK; }
  if(name == "dome_roll_servo_speed") { value = String(params_.dome_roll_servo_speed); return RES_OK; }
  if(name == "dome_roll_servo_invert") { value = String(params_.dome_roll_servo_invert); return RES_OK; }
  if(name == "dome_heading_servo_min") { value = String(params_.dome_heading_servo_min); return RES_OK; }
  if(name == "dome_heading_servo_max") { value = String(params_.dome_heading_servo_max); return RES_OK; }
  if(name == "dome_heading_servo_offset") { value = String(params_.dome_heading_servo_offset); return RES_OK; }
  if(name == "dome_heading_servo_speed") { value = String(params_.dome_heading_servo_speed); return RES_OK; }
  if(name == "dome_heading_servo_invert") { value = String(params_.dome_heading_servo_invert); return RES_OK; }
  return RES_PARAM_NO_SUCH_PARAMETER;
}

Result BB8::setParameterValue(const String& name, const String& value) { 
  Result res; 
  if(name == "drive_speed_factor") { float fval = value.toFloat(); if(fval > 2.0) res = RES_PARAM_INVALID_VALUE; params_.drive_speed_factor = fval; res = RES_OK; }
  if(name == "turn_speed_factor") { float fval = value.toFloat(); if(fval > 2.0) res=  RES_PARAM_INVALID_VALUE; params_.turn_speed_factor = fval; res=  RES_OK; }
  if(name == "body_roll_servo_min") { float fval = value.toFloat(); if(fval < 0 || fval > 360.0) res =  RES_PARAM_INVALID_VALUE; params_.body_roll_servo_min = servolimits[BODY_ROLL_SERVO-1].min = fval; res =  RES_OK; }
  if(name == "body_roll_servo_max") { float fval = value.toFloat(); if(fval < 0 || fval > 360.0) res =  RES_PARAM_INVALID_VALUE; params_.body_roll_servo_max = servolimits[BODY_ROLL_SERVO-1].max = fval; res =  RES_OK; }
  if(name == "body_roll_servo_offset") { float fval = value.toFloat(); if(fval < 0 || fval > 360.0) res =  RES_PARAM_INVALID_VALUE; params_.body_roll_servo_offset = servolimits[BODY_ROLL_SERVO-1].offset = fval; res =  RES_OK; }
  if(name == "body_roll_servo_speed") { float fval = value.toFloat(); if(fval < 0 || fval > 360.0) res =  RES_PARAM_INVALID_VALUE; params_.body_roll_servo_speed = servolimits[BODY_ROLL_SERVO-1].speed = fval; res =  RES_OK; }
  if(name == "body_roll_servo_invert") { bool bval = value == "true" ? true : false; params_.body_roll_servo_invert = bval; res =  RES_OK; }
  if(name == "dome_pitch_servo_min") { float fval = value.toFloat(); if(fval < 0 || fval > 360.0) res =  RES_PARAM_INVALID_VALUE; params_.dome_pitch_servo_min = servolimits[DOME_PITCH_SERVO-1].min = fval; res =  RES_OK; }
  if(name == "dome_pitch_servo_max") { float fval = value.toFloat(); if(fval < 0 || fval > 360.0) res =  RES_PARAM_INVALID_VALUE; params_.dome_pitch_servo_max = servolimits[DOME_PITCH_SERVO-1].max = fval; res =  RES_OK; }
  if(name == "dome_pitch_servo_offset") { float fval = value.toFloat(); if(fval < 0 || fval > 360.0) res =  RES_PARAM_INVALID_VALUE; params_.dome_pitch_servo_offset = fval; servolimits[DOME_PITCH_SERVO-1].offset = fval; res =  RES_OK; }
  if(name == "dome_pitch_servo_speed") { float fval = value.toFloat(); if(fval < 0 || fval > 360.0) res =  RES_PARAM_INVALID_VALUE; params_.dome_pitch_servo_speed = servolimits[DOME_PITCH_SERVO-1].speed = fval; res =  RES_OK; }
  if(name == "dome_pitch_servo_invert") { bool bval = value == "true" ? true : false; params_.dome_pitch_servo_invert = bval; res =  RES_OK; }
  if(name == "dome_roll_servo_min") { float fval = value.toFloat(); if(fval < 0 || fval > 360.0) res =  RES_PARAM_INVALID_VALUE; params_.dome_roll_servo_min = servolimits[DOME_ROLL_SERVO-1].min = fval; res =  RES_OK; }
  if(name == "dome_roll_servo_max") { float fval = value.toFloat(); if(fval < 0 || fval > 360.0) res =  RES_PARAM_INVALID_VALUE; params_.dome_roll_servo_max = servolimits[DOME_ROLL_SERVO-1].max = fval; res =  RES_OK; }
  if(name == "dome_roll_servo_offset") { float fval = value.toFloat(); if(fval < 0 || fval > 360.0) res =  RES_PARAM_INVALID_VALUE; params_.dome_roll_servo_offset = servolimits[DOME_ROLL_SERVO-1].offset = fval; res =  RES_OK; }
  if(name == "dome_roll_servo_speed") { float fval = value.toFloat(); if(fval < 0 || fval > 360.0) res =  RES_PARAM_INVALID_VALUE; params_.dome_roll_servo_speed = servolimits[DOME_ROLL_SERVO-1].speed = fval; res =  RES_OK; }
  if(name == "dome_roll_servo_invert") { bool bval = value == "true" ? true : false; params_.dome_roll_servo_invert = bval; res =  RES_OK; }
  if(name == "dome_heading_servo_min") { float fval = value.toFloat(); if(fval < 0 || fval > 360.0) res =  RES_PARAM_INVALID_VALUE; params_.dome_heading_servo_min = servolimits[DOME_HEADING_SERVO-1].min = fval; res =  RES_OK; }
  if(name == "dome_heading_servo_max") { float fval = value.toFloat(); if(fval < 0 || fval > 360.0) res =  RES_PARAM_INVALID_VALUE; params_.dome_heading_servo_max = servolimits[DOME_HEADING_SERVO-1].max = fval; res =  RES_OK; }
  if(name == "dome_heading_servo_offset") { float fval = value.toFloat(); if(fval < 0 || fval > 360.0) res =  RES_PARAM_INVALID_VALUE; params_.dome_heading_servo_offset = servolimits[DOME_HEADING_SERVO-1].offset = fval; res =  RES_OK; }
  if(name == "dome_heading_servo_speed") { float fval = value.toFloat(); if(fval < 0 || fval > 360.0) res =  RES_PARAM_INVALID_VALUE; params_.dome_heading_servo_speed = servolimits[DOME_HEADING_SERVO-1].speed = fval; res =  RES_OK; }
  if(name == "dome_heading_servo_invert") { bool bval = value == "true" ? true : false; params_.dome_heading_servo_invert = bval; res =  RES_OK; }

  if(res == RES_OK) ConfigStorage::storage.writeBlock(paramsHandle_, (uint8_t*)&params_);
  return res;
}


void BB8::printStatus(ConsoleStream *stream) {
  char buf[255];

  float r, p, h;
  BB8BodyIMU::imu.getFilteredRPH(r, p, h);

  sprintf(buf, "Received: %5d Missed: %2d Seq: %d Buttons: %c%c%c%c%c Axes: %4d %4d %4d %4d %4d Gyro: R%.2f P%.2f H%.2f",
    packetsReceived_, packetsMissed_, lastPacket_.seqnum,
    lastPacket_.payload.cmd.button0 ? 'X' : '_',
    lastPacket_.payload.cmd.button1 ? 'X' : '_',
    lastPacket_.payload.cmd.button2 ? 'X' : '_',
    lastPacket_.payload.cmd.button3 ? 'X' : '_',
    lastPacket_.payload.cmd.button4 ? 'X' : '_',
    lastPacket_.payload.cmd.getAxis(0),
    lastPacket_.payload.cmd.getAxis(1),
    lastPacket_.payload.cmd.getAxis(2),
    lastPacket_.payload.cmd.getAxis(3),
    lastPacket_.payload.cmd.getAxis(4),
    r, p, h);

  if(stream != NULL) stream->print(buf);
  else Console::console.printBroadcast(buf);
}