#if !defined(DOIMU_H)
#define DOIMU_H

#include <LibBB.h>

#include <math.h>
#include <Adafruit_ISM330DHCX.h>
#include <MadgwickAHRS.h>

#include "DOConfig.h"

using namespace bb;

class DOIMU {
public:
  static DOIMU imu;

  bool begin();

  bool available() { return available_; }

  bool calibrateGyro(ConsoleStream *stream=NULL, int milliseconds = 2000, int step = 10);

  bool integrateGyroMeasurement(bool reset = false);
  int getIntegratedGyroMeasurement(float& r, float& p, float& h);

  bool getGyroMeasurement(float& r, float& p, float& h, bool calibrated=true);
  bool getAccelMeasurement(float& x, float& y, float& z, int32_t& timestamp);

  virtual bool update();
  bool getFilteredRPH(float& r, float& p, float& h);

  IMUState getIMUState();
  void printStats(const arduino::String& prefix = "");
private:  
  DOIMU();

  Madgwick madgwick_;
  bool available_;
  Adafruit_ISM330DHCX imu_;
  Adafruit_Sensor *temp_, *accel_, *gyro_;
  float calR_, calP_, calH_;
  float lastR_, lastP_, lastH_;
  float lastX_, lastY_, lastZ_;
  float intR_, intP_, intH_;
  int intNum_;
  int32_t intLastTS_;
  bool intRunning_ = false;
};

#endif // DOIMU_H