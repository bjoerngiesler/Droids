#if !defined(DODROID_H)
#define DODROID_H

#include <LibBB.h>

using namespace bb;

class DODroid: public Subsystem, public PacketReceiver {
public:
  static DODroid droid;
  
  struct Params {
    float speedKp, speedKi, speedKd;
    float posKp, posKi, posKd;
    float driveAccel;
  };
  static Params params_;

  DODroid();
  virtual Result initialize();
  virtual Result start(ConsoleStream *stream = NULL);
  virtual Result stop(ConsoleStream *stream = NULL);
	virtual Result step();

  virtual void printStatus(ConsoleStream *stream);
  virtual Result fillAndSendStatusPacket();

  virtual Result incomingPacket(const Packet& packet);
  virtual Result handleConsoleCommand(const std::vector<String>& words, ConsoleStream *stream);
  virtual Result setParameterValue(const String& name, const String& stringVal);

protected:
  bb::DCMotor leftMotor, rightMotor;
  bb::EncoderControlInput leftEncoder, rightEncoder;
  bb::PIDController leftController, rightController;
};

#endif