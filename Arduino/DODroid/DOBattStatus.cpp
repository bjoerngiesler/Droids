#include "DOBattStatus.h"
#include "DOConfig.h"
#include <Adafruit_INA219.h>

DOBattStatus DOBattStatus::batt;

static Adafruit_INA219 ina(BATT_STATUS_ADDR);

DOBattStatus::DOBattStatus() {
  voltage_ = 0;
  current_ = 0;
  available_ = false;
}

bool DOBattStatus::begin() {
  ina.begin();
  available_ = true;
  return true;
}
  
bool DOBattStatus::available() {
  return available_;
}

bool DOBattStatus::updateVoltage() {
  if(!available_) return false;

#if defined(BATT_VOLTAGE_PRECISE)
  voltage_ = ina.getShuntVoltage_mV() / 1000 + ina.getBusVoltage_V();
#else 
  voltage_ = ina.getBusVoltage_V();
#endif
  if(isnan(voltage_) || !isfinite(voltage_)) {
    available_ = false;
    voltage_ = -1;
    return false;
  }
  return true;
}

bool DOBattStatus::updateCurrent() {
  if(!available_) return false;
  current_ = ina.getCurrent_mA();
  if(isnan(current_) || !isfinite(current_)) {
    available_ = false;
    current_ = -1;
    return false;
  }
  return true;
}

float DOBattStatus::current() {
  return current_;
}

float DOBattStatus::voltage() {
  return voltage_;
}

bb::BatteryState DOBattStatus::getBatteryState() {
  bb::BatteryState batteryState;
  if(!available_) {
    batteryState.errorState = bb::ERROR_NOT_PRESENT;
    return batteryState;
  }

  batteryState.errorState = bb::ERROR_OK;
  batteryState.voltage = voltage_;
  batteryState.current = current_;
  return batteryState;
}

