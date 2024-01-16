#if !defined(BB8XBEE_H)
#define BB8XBEE_H

#include <Arduino.h>
#include <vector>
#include "BBSubsystem.h"
#include "BBConfigStorage.h"
#include "BBPacket.h"

#define DEFAULT_CHAN    0xC
#define DEFAULT_PAN     0x3332

#define DEFAULT_STATION_DROID 0x1
#define DEFAULT_STATION_LEFT_REMOTE 0x2
#define DEFAULT_STATION_RIGHT_REMOTE 0x3
#define DEFAULT_BPS     9600

namespace bb {

class XBee: public Subsystem {
public:
	static XBee xbee;

	enum StationType {
		STATION_DROID      = 0,
		STATION_REMOTE     = 1,
		STATION_DIAGNOSTIC = 2,
		STATION_PAIRING    = 3
	};

	enum DroidType {
		DROID_STATIC        = 0,
		DROID_DIFF_STABLE   = 1,
		DROID_DIFF_UNSTABLE = 2,
		DROID_DRIVE_TILT    = 3,
		DROID_HOLONOMOUS    = 4,
		DROID_4LEG_WALKING  = 5,
		DROID_2LEG_WALKING  = 6,
		DROID_RESERVED      = 7
	};

	enum RemoteType {
		REMOTE_BAVARIAN_L = 0,
		REMOTE_BAVARIAN_R = 1,
		REMOTE_RESERVED1  = 2,
		REMOTE_RESERVED2  = 3,
		REMOTE_RESERVED3  = 4,
		REMOTE_RESERVED4  = 5,
		REMOTE_RESERVED5  = 6,
		REMOTE_RESERVED6  = 7
	};

	static uint16_t makeStationID(DroidType dtype, uint8_t builderid, uint8_t stationid) {
		return STATION_DROID << 14 | dtype << 11 | (builderid & 0x7f) << 4 | (stationid & 0x0f);
	}
	static uint16_t makeStationID(RemoteType rtype, uint8_t builderid, uint8_t stationid) {
		return STATION_REMOTE << 14 | rtype << 11 | (builderid & 0x7f) << 4 | (stationid & 0x0f);
	}
	static StationType stationTypeFromId(uint16_t id) {
		return (StationType)((id>>14)&0x3);
	}
	static DroidType droidTypeFromId(uint16_t id) {
		return (DroidType)((id>>11)&0x7);
	}
	static RemoteType remoteTypeFromId(uint16_t id) {
		return (RemoteType)((id>>11)&0x7);
	}
	static uint8_t builderIDFromId(uint16_t id) {
		return (id>>4)&0x7f;
	}
	static uint8_t stationIDFromId(uint16_t id) {
		return id&0x7;
	}


	virtual Result start(ConsoleStream *stream = NULL);
	virtual Result stop(ConsoleStream *stream = NULL);
	virtual Result step();
	virtual Result parameterValue(const String& name, String& value);
	virtual Result setParameterValue(const String& name, const String& value);
	virtual Result initialize(uint8_t chan, uint16_t pan, uint16_t station, uint16_t partner, uint32_t bps, HardwareSerial *uart=&Serial1);
	virtual Result handleConsoleCommand(const std::vector<String>& words, ConsoleStream *stream);

	void setPacketMode(bool onoff);
	Result addPacketReceiver(PacketReceiver *receiver);
	Result removePacketReceiver(PacketReceiver *receiver);

	void setChannel(uint8_t chan) { params_.chan = chan; }
	void setPAN(uint16_t pan) { params_.pan = pan; }
	void setStation(uint16_t station) { params_.station = station; }
	void setPartner(uint16_t partner) { params_.partner = partner; }
	void setBPS(uint32_t bps) { params_.bps = bps; }
	void setName(const char* name) { memset(params_.name, 0, 20); snprintf(params_.name, 19, name); }

	Result enterATModeIfNecessary(ConsoleStream *stream = NULL);
	Result leaveATMode(ConsoleStream *stream = NULL); // incurs a mandatory delay of 1000ms!
	bool isInATMode();
	Result changeBPSTo(uint32_t bps, ConsoleStream *stream=NULL, bool stayInAT=false);
	int getCurrentBPS() { return currentBPS_; }
	Result setConnectionInfo(uint8_t chan, uint16_t pan, uint16_t station, uint16_t partner, bool stayInAT=false);
	Result getConnectionInfo(uint8_t& chan, uint16_t& pan, uint16_t& station, uint16_t& partner, bool stayInAT=false);

	struct Node {
		uint16_t stationId;
		uint64_t address;
		uint8_t rssi;
		char name[20];
	};
	Result discoverNodes(std::vector<Node>& nodes);

	Result send(const String& str);
	Result send(const uint8_t *bytes, size_t size);
	Result send(const Packet& packet);
	bool available();
	String receive();
	Result receiveAndHandlePacket();

	typedef enum {
		DEBUG_SILENT = 0,
		DEBUG_PROTOCOL   = 0x01,
		DEBUG_XBEE_COMM  = 0x02
	} DebugFlags;

	void setDebugFlags(DebugFlags);

protected:
	XBee();
	virtual ~XBee();

	DebugFlags debug_;
	int timeout_;
	unsigned long atmode_millis_, atmode_timeout_;
	bool atmode_, stayInAT_;
	HardwareSerial* uart_;
	unsigned int currentBPS_;

	int chan_, pan_, station_, partner_, bps_;

	typedef struct {
		uint8_t chan;
		uint16_t pan, station, partner;
		uint32_t bps;
		char name[20];
	} XBeeParams;
	XBeeParams params_;
	ConfigStorage::HANDLE paramsHandle_;
	std::vector<PacketReceiver*> receivers_;

	bool sendContinuous_;
	int continuous_;
	bool packetMode_; // if false, string mode
	uint8_t packetBuf_[255];
	size_t packetBufPos_;


	String sendStringAndWaitForResponse(const String& str, int predelay=0, bool cr=true);
	bool sendStringAndWaitForOK(const String& str, int predelay=0, bool cr=true);
	bool readString(String& str, unsigned char terminator='\r');
};

};

#endif // BB8XBEE_H