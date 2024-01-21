#include <limits.h> // for ULONG_MAX
#include <inttypes.h> // for uint64_t format string
#include <vector>

#include "BBXBee.h"
#include "BBError.h"
#include "BBConsole.h"
#include "BBRunloop.h"

bb::XBee bb::XBee::xbee;

static std::vector<int> baudRatesToTry = { 115200, 9600, 19200, 28800, 38400, 57600, 76800 }; // start with 115200, then try 9600

bb::XBee::XBee() {
	uart_ = &Serial1;
	debug_ = (XBee::DebugFlags)(DEBUG_PROTOCOL);
	timeout_ = 1000;
	atmode_ = false;
	atmode_millis_ = 0;
	atmode_timeout_ = 10000;
	currentBPS_ = 0;
	memset(packetBuf_, 0, sizeof(packetBuf_));
	packetBufPos_ = 0;
	apiMode_ = false;

	name_ = "xbee";
	description_ = "Communication via XBee 802.5.14";
	help_ = "In order for communication to work, the PAN and channel numbers must be identical.\r\n" \
	"Available commands:\r\n" \
	"\tsend <string>:      Send <string> to partner\r\n"\
	"\tsend_control_packet:    Send a zero control packet (with sequence number and CRC set) to partner\r\n"
	"\tcontinuous on|off:  Start or stop sending a continuous stream of numbers (or zero command packets when in packet mode)\r\n"\
	"\tpacket_mode on|off: Switch to packet mode\r\n" \
	"\tapi_mode on|off: Enter / leave API mode\r\n" \
	"\tsend_api_packet <dest>: Send zero control packet to destination\r\n";

	addParameter("channel", "Communication channel (between 11 and 26, usually 12)", params_.chan, 11, 26);
	addParameter("pan", "Personal Area Network ID (16bit, 65535 is broadcast)", params_.pan, 0, 65535);
	addParameter("station", "Station ID (MY) for this device (16bit)", params_.station, 0, 65535);
	addParameter("partner", "Partner ID this device should talk to (16bit)", params_.partner, 0, 65535);
	addParameter("bps", "Communication bps rate", params_.bps, 0, 200000);

	paramsHandle_ = ConfigStorage::storage.reserveBlock(sizeof(XBeeParams));
	if(ConfigStorage::storage.blockIsValid(paramsHandle_)) ConfigStorage::storage.readBlock(paramsHandle_, (uint8_t*)&params_);
}

bb::XBee::~XBee() {
}

bb::Result bb::XBee::initialize(uint8_t chan, uint16_t pan, uint16_t station, uint16_t partner, uint32_t bps, HardwareSerial *uart) {
	if(operationStatus_ != RES_SUBSYS_NOT_INITIALIZED) return RES_SUBSYS_ALREADY_INITIALIZED;

	paramsHandle_ = ConfigStorage::storage.reserveBlock(sizeof(params_));
	if(ConfigStorage::storage.blockIsValid(paramsHandle_)) {
		Console::console.printfBroadcast("XBee: Storage block is valid\n");
		ConfigStorage::storage.readBlock(paramsHandle_, (uint8_t*)&params_);
	} else {
		Console::console.printfBroadcast("XBee: Storage block is invalid, using passed parameters\n");
		memset(&params_, 0, sizeof(params_));
		params_.chan = chan;
		params_.pan = pan;
		params_.station = station;
		params_.partner = partner;
		params_.bps = bps;
	}

	uart_ = uart;

	operationStatus_ = RES_SUBSYS_NOT_STARTED;
	return Subsystem::initialize();	
}

bb::Result bb::XBee::start(ConsoleStream *stream) {
	if(isStarted()) return RES_SUBSYS_ALREADY_STARTED;
	if(NULL == uart_) return RES_SUBSYS_HW_DEPENDENCY_MISSING;

	if(stream == NULL) stream = Console::console.serialStream();

	currentBPS_ = 0;
	if(currentBPS_ == 0) {
		if(stream) stream->printf("auto-detecting BPS... ");
		
		for(size_t i=0; i<baudRatesToTry.size(); i++) {
			if(stream) {
				stream->printf("%d...", baudRatesToTry[i]); 
			}
			
			uart_->begin(baudRatesToTry[i]);
			
			if(enterATModeIfNecessary() == RES_OK) {
				currentBPS_ = baudRatesToTry[i];
				break; 
			}
		}
		if(currentBPS_ == 0) {
			if(stream) stream->printf("failed.\n");
			operationStatus_ = RES_SUBSYS_HW_DEPENDENCY_MISSING;
			return RES_SUBSYS_HW_DEPENDENCY_MISSING;
		}
	} else {
		if(stream) stream->printf("Using %dbps.", currentBPS_);
		uart_->begin(currentBPS_);
		if(enterATModeIfNecessary() != RES_OK) {
			operationStatus_ = RES_SUBSYS_HW_DEPENDENCY_MISSING;
			return RES_SUBSYS_HW_DEPENDENCY_MISSING;
		}
	}

	String addrH = sendStringAndWaitForResponse("ATSH"); addrH.trim();
	String addrL = sendStringAndWaitForResponse("ATSL"); addrL.trim();
	String fw = sendStringAndWaitForResponse("ATVR"); fw.trim();
	if(addrH == "" || addrL == "" || fw == "") return RES_SUBSYS_COMM_ERROR;

	if(stream) {
		stream->printf("Found XBee at address: %s:%s Firmware version: %s\n", addrH.c_str(), addrL.c_str(), fw.c_str());
	}

	String retval = sendStringAndWaitForResponse("ATCT"); 
	if(retval != "") {
		atmode_timeout_ = strtol(retval.c_str(), 0, 16) * 100;
		if(stream!=NULL) stream->printf("Command timeout: %d\n", atmode_timeout_);
	}
	if(atmode_timeout_ != 1000) {
		if(sendStringAndWaitForOK("ATCT=a") == false) {
			if(stream) stream->printf("Couldn't set AT Mode Timeout\n");
			return RES_SUBSYS_COMM_ERROR;
		}

		// need to leave and jump back in here to make sure the new timeout is used
		leaveATMode();
		enterATModeIfNecessary();
		atmode_timeout_ = 1000;
	}

	if(setConnectionInfo(params_.chan, params_.pan, params_.station, params_.partner, true) != RES_OK) {
		if(stream) stream->printf("Setting connection info failed.\n");
		return RES_SUBSYS_COMM_ERROR;
	} else {
		if(stream) stream->printf("Setting connection info successful.\n");
	}

	String str = String("ATNI") + params_.name;
	if(sendStringAndWaitForOK(str) == false) {
		Console::console.printfBroadcast("Error setting name\n");
		return RES_SUBSYS_COMM_ERROR;
	} 

	if(sendStringAndWaitForOK("ATNT=64") == false) {
		Console::console.printfBroadcast("Error setting node discovery timeout\n");
		return RES_SUBSYS_COMM_ERROR;
	} 

	bool changedBPS = false;

	if(params_.bps != currentBPS_) {
		if(changeBPSTo(params_.bps, stream, true) != RES_OK) {
			if(stream) stream->printf("Setting BPS failed.\n"); 
			else Console::console.printfBroadcast("Setting BPS failed.\n"); 
			currentBPS_ = 0;
			return RES_SUBSYS_COMM_ERROR;
		} else {
			Console::console.printfBroadcast("Setting BPS to %d successful.\n", params_.bps); 
			changedBPS = true;
		}
	}

	if(sendStringAndWaitForOK("ATWR") == false) {
		if(stream) stream->printf("Couldn't write config!\n");
		return RES_SUBSYS_COMM_ERROR;
	}
	leaveATMode();

	// we have changed the BPS successfully?
	if(changedBPS) {
		if(stream) stream->printf("Closing and reopening serial at %dbps\n", params_.bps);
		uart_->end();
		uart_->begin(params_.bps);

		enterATModeIfNecessary();
		if(sendStringAndWaitForOK("AT") == false) return RES_SUBSYS_COMM_ERROR;
		currentBPS_ = params_.bps;
	}
	leaveATMode();

	operationStatus_ = RES_OK;
	started_ = true;

	return RES_OK;
}

bb::Result bb::XBee::stop(ConsoleStream *stream) {
	if(stream) stream = stream; // make compiler happy
	operationStatus_ = RES_SUBSYS_NOT_STARTED;
	currentBPS_ = 0;
	started_ = false;
	return RES_OK;
}

bb::Result bb::XBee::step() {
	while(available()) {
		if(apiMode_) {
			Result retval = receiveAndHandleAPIMode();
			if(retval != RES_OK) return retval;
		} else if(packetMode_) {
			Result retval = receiveAndHandlePacket();
			if(retval != RES_OK) return retval;
		} else {
			bb::Console::console.printfBroadcast("%s\n", receive().c_str());
		}
	}

	if(sendContinuous_) {
		if(packetMode_) {

			Packet packet;
			memset(&packet, 0, sizeof(packet));
			packet.type = PACKET_TYPE_CONTROL;
			packet.source = PACKET_SOURCE_TEST_ONLY;

			return send(packet);
		} else {
			String str(continuous_++);
			send(str);			
		}
	}

	return RES_OK;
}

bb::Result bb::XBee::parameterValue(const String& name, String& value) {
	if(name == "channel") { 
		value = String(params_.chan); return RES_OK; 
	} else if(name == "pan") {
		value = String(params_.pan); return RES_OK;
	} else if(name == "station") {
		value = String(params_.station); return RES_OK;
	} else if(name == "partner") {
		value = String(params_.partner); return RES_OK;
	} else if(name == "bps") {
		value = String(params_.bps); return RES_OK;
	} 

	return RES_PARAM_NO_SUCH_PARAMETER;
}

bb::Result bb::XBee::setParameterValue(const String& name, const String& value) {
	Result res = RES_PARAM_NO_SUCH_PARAMETER;

	if(name == "channel") { 
		params_.chan = value.toInt();
		res = setConnectionInfo(params_.chan, params_.pan, params_.station, params_.partner, false);
	} else if(name == "pan") {
		params_.pan = value.toInt();
		res = setConnectionInfo(params_.chan, params_.pan, params_.station, params_.partner, false);
	} else if(name == "station") {
		params_.station = value.toInt();
		res = setConnectionInfo(params_.chan, params_.pan, params_.station, params_.partner, false);
	} else if(name == "partner") {
		params_.partner = value.toInt();
		res = setConnectionInfo(params_.chan, params_.pan, params_.station, params_.partner, false);
	} else if(name == "bps") {
		params_.bps = value.toInt();
		res = RES_OK;
	}

	if(res == RES_OK) {
		ConfigStorage::storage.writeBlock(paramsHandle_, (uint8_t*)&params_);
	}

	return res;
}

bb::Result bb::XBee::handleConsoleCommand(const std::vector<String>& words, ConsoleStream *stream) {
	if(words.size() == 0) return RES_CMD_UNKNOWN_COMMAND;
	
	if(words[0] == "send") {
		if(words.size() != 2) return RES_CMD_INVALID_ARGUMENT_COUNT;
		return send(words[1]);
	} 

	else if(words[0] == "send_control_packet") {
		if(words.size() != 1) return RES_CMD_INVALID_ARGUMENT;

		Packet packet;
		memset(&packet, 0, sizeof(packet));
		packet.type = PACKET_TYPE_CONTROL;
		packet.source = PACKET_SOURCE_TEST_ONLY;

		return send(packet);
	} 

	else if(words[0] == "send_api_packet") {
		if(words.size() != 2) return RES_CMD_INVALID_ARGUMENT;
		if(!apiMode_) {
			Console::console.printfBroadcast("Must be in API mode to do this.\n");
			return RES_SUBSYS_WRONG_MODE;
		}

		uint16_t dest = words[1].toInt();

		Packet packet;
		memset(&packet, 0, sizeof(packet));
		packet.type = PACKET_TYPE_CONTROL;
		packet.source = PACKET_SOURCE_TEST_ONLY;

		return sendTo(dest, packet, true);
	} 
	else if(words[0] == "continuous") {
		if(words.size() != 2) return RES_CMD_INVALID_ARGUMENT_COUNT;
		else {
			if(words[1] == "on" || words[1] == "true") {
				sendContinuous_ = true;
				continuous_ = 0;
				return RES_OK;
			}
			else if(words[1] == "off" || words[1] == "false") {
				sendContinuous_ = false;
				return RES_OK;
			}
			else return RES_CMD_INVALID_ARGUMENT;	
		}
	} 

	else if(words[0] == "packet_mode") {
		if(words.size() != 2) return RES_CMD_INVALID_ARGUMENT_COUNT;
		else {
			if(words[1] == "on" || words[1] == "true") {
				setPacketMode(true);
				sendContinuous_ = false;
				return RES_OK;
			}
			else if(words[1] == "off" || words[1] == "false") {
				setPacketMode(false);
				return RES_OK;
			}
			else return RES_CMD_INVALID_ARGUMENT;	
		}
	} else if(words[0] == "api_mode") {
		if(words.size() != 2) return RES_CMD_INVALID_ARGUMENT_COUNT;
		else {
			if(words[1] == "on" || words[1] == "true") {
				return setAPIMode(true);
			}
			else if(words[1] == "off" || words[1] == "false") {
				return setAPIMode(false);
			}
			else return RES_CMD_INVALID_ARGUMENT;	
		}		
	}

	return bb::Subsystem::handleConsoleCommand(words, stream);
}

void bb::XBee::setPacketMode(bool onoff) {
	packetMode_ = onoff;
}

bb::Result bb::XBee::setAPIMode(bool onoff) {
	Result res;
	if(onoff == true) {
		if(apiMode_ == true) {
			Console::console.printfBroadcast("Already in API mode\n");
			return RES_CMD_INVALID_ARGUMENT;
		}

		enterATModeIfNecessary();
		if(sendStringAndWaitForOK("ATAP=2") == true) {
			apiMode_ = true;
			leaveATMode();
			return RES_OK;
		} else {
			leaveATMode();
			return RES_SUBSYS_COMM_ERROR;
		}
	} else {
		if(apiMode_ == false) {
			Console::console.printfBroadcast("Not in API mode\n");
			return RES_CMD_INVALID_ARGUMENT;			
		}

		res = sendAPIModeATCommand(1, "AP", 0);
		if(res == RES_OK) apiMode_ = false;
		return res;
	}
}


bb::Result bb::XBee::addPacketReceiver(PacketReceiver *receiver) {
	for(size_t i=0; i<receivers_.size(); i++)
		if(receivers_[i] == receiver)
			return RES_COMMON_DUPLICATE_IN_LIST;
	receivers_.push_back(receiver);
	return RES_OK;
}

bb::Result bb::XBee::removePacketReceiver(PacketReceiver *receiver) {
	for(size_t i=0; i<receivers_.size(); i++)
		if(receivers_[i] == receiver) {
			receivers_.erase(receivers_.begin()+i);
			return RES_OK;
		}
	return RES_COMMON_NOT_IN_LIST;
}


bb::Result bb::XBee::enterATModeIfNecessary(ConsoleStream *stream) {
	if(isInATMode()) {
		return RES_OK;
	}

	if(debug_ && stream!=NULL) {
		stream->printf("Entering AT mode.\n");
		stream->printf("Wait 1s... ");
	}

	int numDiscardedBytes = 0;

	for(int timeout = 0; timeout < 1000; timeout++) {
		while(uart_->available())  {
			uart_->read();
			numDiscardedBytes++;
		}
		delay(1);
	}

	if(debug_ && stream!=NULL) 
		stream->printf("Sending +++... \n");

	uart_->write("+++");

	bool success = false;

	for(int timeout = 0; timeout < 1200; timeout++) {
		unsigned char c;

		while(uart_->available()) {
			c = uart_->read(); 
			if(c == 'O') {
				if(!uart_->available()) delay(1);
				if(!uart_->available()) continue;
				c = uart_->read();
				if(c == 'K') {
					if(!uart_->available()) delay(1);
					if(!uart_->available()) continue;
					c = uart_->read();
					if(c == '\r') {
						success = true;
						break;
					} else numDiscardedBytes++;
				} else numDiscardedBytes++;
			} else numDiscardedBytes++;
		}

		delay(1);
	}


	if(success) {
		if(debug_ && stream!=NULL) {
			stream->printf("Successfully entered AT Mode\n");
		}
		if(numDiscardedBytes) {
			Console::console.printfBroadcast("Discarded %d bytes while entering AT mode.\n", numDiscardedBytes);
		}
		atmode_millis_ = millis();
		atmode_ = true;
				
		return RES_OK;
	}

	if(debug_ && stream!=NULL) stream->printf("no response to +++\n");
	return RES_COMM_TIMEOUT;
}

bool bb::XBee::isInATMode() {
	if(atmode_ == false) return false;
	unsigned long m = millis();
	if(atmode_millis_ < m) {
		if(m - atmode_millis_ < atmode_timeout_) return true;
	} else {
		if(ULONG_MAX - m + atmode_millis_ < atmode_timeout_) return true;
	}
	atmode_ = false;
	return false;
}

bb::Result bb::XBee::leaveATMode(ConsoleStream *stream) {
	if(!isInATMode()) {
		return RES_OK;
	}
	if((debug_ & DEBUG_PROTOCOL) && stream!=NULL) stream->printf("Sending ATCN to leave AT Mode\n");
	if(sendStringAndWaitForOK("ATCN") == true) {
		if((debug_ & DEBUG_PROTOCOL) && stream!=NULL) stream->printf("Successfully left AT Mode\n");
		atmode_ = false;
		delay(1100);
		return RES_OK;
	}

	if((debug_ & DEBUG_PROTOCOL) && stream!=NULL) stream->printf("no response to ATCN\n");
	return RES_COMM_TIMEOUT;
}

bb::Result bb::XBee::changeBPSTo(uint32_t bps, ConsoleStream *stream, bool stayInAT) {
	uint32_t paramVal;
	switch(bps) {
	case 1200:   paramVal = 0x0; break;
	case 2400:   paramVal = 0x1; break;
	case 4800:   paramVal = 0x2; break;
	case 9600:   paramVal = 0x3; break;
	case 19200:  paramVal = 0x4; break;
	case 38400:  paramVal = 0x5; break;
	case 57600:  paramVal = 0x6; break;
	case 115200: paramVal = 0x7; break;
	case 230400: paramVal = 0x8; break;
	case 460800: paramVal = 0x9; break;
	case 921600: paramVal = 0xa; break;
	default:     paramVal = bps; break;
	}

#if 0
	String retval = sendStringAndWaitForResponse("ATBD");

	if((unsigned)retval.toInt() == paramVal) {
		if(stream) stream->printf("XBee says it's already running at %d bps\n", bps);
		return RES_OK;
	}
#endif

	if(stream) stream->printf("Sending ATBD=%x\n", paramVal);
	if(sendStringAndWaitForOK(String("ATBD=")+String(paramVal, HEX)) == false) return RES_SUBSYS_COMM_ERROR;

	if(stayInAT == false) leaveATMode();
	currentBPS_ = bps;

	return RES_OK;
}

bb::Result bb::XBee::setConnectionInfo(uint8_t chan, uint16_t pan, uint16_t station, uint16_t partner, bool stayInAT) {
	params_.chan = chan;
	params_.pan = pan;
	params_.station = station;
	params_.partner = partner;

	enterATModeIfNecessary();

	if(sendStringAndWaitForOK(String("ATCH=")+String(chan, HEX)) == false) {
		if(debug_ & DEBUG_PROTOCOL) Console::console.printfBroadcast("ERROR: Setting Channel failed!\n");
		if(!stayInAT) leaveATMode();
		return RES_COMM_TIMEOUT;
	} 
	delay(10);
	if(sendStringAndWaitForOK(String("ATID=")+String(pan, HEX)) == false) {
		if(debug_ & DEBUG_PROTOCOL) Console::console.printfBroadcast("ERROR: Setting PAN failed!\n");
		if(!stayInAT) leaveATMode();
		return RES_COMM_TIMEOUT;
	}
	delay(10);
	if(sendStringAndWaitForOK(String("ATMY=")+String(station, HEX)) == false) {
		if(debug_ & DEBUG_PROTOCOL) Console::console.printfBroadcast("ERROR: Setting MY failed!\n");
		if(!stayInAT) leaveATMode();
		return RES_COMM_TIMEOUT;
	}
	delay(10);
	if(sendStringAndWaitForOK(String("ATDL=")+String(partner, HEX)) == false) {
		if(debug_ & DEBUG_PROTOCOL) Console::console.printfBroadcast("ERROR: Setting Destination Low failed!\n");
		if(!stayInAT) leaveATMode();
		return RES_COMM_TIMEOUT;
	}
	delay(10);

	if(!stayInAT) leaveATMode();

	return RES_OK;
}

bb::Result bb::XBee::getConnectionInfo(uint8_t& chan, uint16_t& pan, uint16_t& station, uint16_t& partner, bool stayInAT) {
	enterATModeIfNecessary();

	String retval;

	retval = sendStringAndWaitForResponse("ATCH");
	if(retval == "") return RES_COMM_TIMEOUT;
	chan = strtol(retval.c_str(), 0, 16);
	delay(10);

	retval = sendStringAndWaitForResponse("ATID");
	if(retval == "") return RES_COMM_TIMEOUT;
	pan = strtol(retval.c_str(), 0, 16);
	delay(10);

	retval = sendStringAndWaitForResponse("ATDL");
	if(retval == "") return RES_COMM_TIMEOUT;
	partner = strtol(retval.c_str(), 0, 16);
	delay(10);

	retval = sendStringAndWaitForResponse("ATMY");
	if(retval == "") return RES_COMM_TIMEOUT;
	station = strtol(retval.c_str(), 0, 16);
	delay(10);

	if(!stayInAT) leaveATMode();
	return RES_OK;
}

bb::Result bb::XBee::discoverNodes(std::vector<bb::XBee::Node>& nodes) {
	if(operationStatus_ != RES_OK) return RES_SUBSYS_NOT_OPERATIONAL;
	if(apiMode_ == false) return RES_SUBSYS_WRONG_MODE;

	nodes = std::vector<bb::XBee::Node>();

	Result res;
	APIFrame request = APIFrame::atRequest(0x5, ('N'<<8 | 'D'));
	res = send(request);
	if(res != RES_OK) return res;

	int timeout = 10000;

	while(timeout > 0) {
		delay(1);
		timeout--;

		if(available()) {
			Result res;
			APIFrame response;
			res = receive(response);
			if(res != RES_OK) {
				Console::console.printfBroadcast("Error receiving response: %s\n", errorMessage(res));
				continue;
			}

			uint8_t frameID, status;
			uint16_t command, length;
			uint8_t *data;

			res = response.unpackATResponse(frameID, command, status, &data, length);
			if(res != RES_OK) {
				Console::console.printfBroadcast("Error unpacking response: %s\n", errorMessage(res));
				continue;
			}

			if(status != 0) {
				Console::console.printfBroadcast("Response with status %d\"", status);
				continue;
			}

			if(length < APIFrame::ATResponseNDMinLength) {
				Console::console.printfBroadcast("Expected >=%d bytes, found %d\n", APIFrame::ATResponseNDMinLength, length);
				continue;
			}

			for(int i=0; i<length; i++) Console::console.printfBroadcast("%d:%x ", i, data[i]);
			Console::console.printfBroadcast("\n");

			APIFrame::ATResponseND *r = (APIFrame::ATResponseND*)data;
			Node n;
			n.stationId = r->my;
			n.address = r->address;
			n.rssi = r->rssi;
			Console::console.printfBroadcast("%x\n", r->name[0]);
			memset(n.name, 0, sizeof(n.name));
			if(length > APIFrame::ATResponseNDMinLength) {
				strncpy(n.name, r->name, strlen(r->name));
			}

			Console::console.printfBroadcast("Discovered station 0x%x at address 0x%llx, RSSI %d, name \"%s\"\n", n.stationId, n.address, n.rssi, n.name);
			nodes.push_back(n);	
		}
	}

	return RES_OK;
}

void bb::XBee::setDebugFlags(DebugFlags debug) {
	debug_ = debug;
}

bb::Result bb::XBee::send(const String& str) {
	if(operationStatus_ != RES_OK) return RES_SUBSYS_NOT_OPERATIONAL;
	if(packetMode_) return RES_SUBSYS_WRONG_MODE;
	if(isInATMode()) leaveATMode();
	uart_->write(str.c_str(), str.length());
	uart_->flush();
	return RES_OK;
}

bb::Result bb::XBee::send(const uint8_t *bytes, size_t size) {
	if(operationStatus_ != RES_OK) return RES_SUBSYS_NOT_OPERATIONAL;
	if(packetMode_) return RES_SUBSYS_WRONG_MODE;
	if(isInATMode()) leaveATMode();
	uart_->write(bytes, size);
	uart_->flush();
	return RES_OK;
}

bb::Result bb::XBee::send(const bb::Packet& packet) {
	if(operationStatus_ != RES_OK) return RES_SUBSYS_NOT_OPERATIONAL;
	if(!packetMode_) return RES_SUBSYS_WRONG_MODE;
	if(isInATMode()) leaveATMode();

	bb::PacketFrame frame;
	frame.packet = packet;

	uint8_t *buf = (uint8_t*)&frame.packet;
	for(size_t i=0; i<sizeof(frame.packet)-1; i++) {
		if(buf[i] & 0x80) {
			Console::console.printfBroadcast("ERROR: Byte %d of packet has highbit set! Not sending.\n", i);
			return RES_PACKET_INVALID_PACKET;
		}
	}

	frame.packet.seqnum = Runloop::runloop.getSequenceNumber() % MAX_SEQUENCE_NUMBER;
	frame.crc = calculateCRC(frame.packet);
	frame.highbit = 1;

	uart_->write((uint8_t*)&frame, sizeof(frame));
	uart_->flush();

	return RES_OK;
}

bb::Result bb::XBee::sendTo(uint16_t dest, const bb::Packet& packet, bool ack) {
	uint8_t buf[14+sizeof(packet)];

	buf[0] = 0x10; // transmit request
	buf[1] = 0x0;  // no response frame
	for(int i=2; i<10; i++) buf[i] = 0xff; 	// We use 16bit addressing, 64bit dest gets set to 0xffffffffffffffff
	buf[10] = (dest >> 8) & 0xff;          	// 16bit dest address
	buf[11] = dest & 0xff;
	buf[12] = 0; 							// broadcast radius - unused
	if(ack == false) {
		buf[13] = 1;						// disable ACK
	} else {
		buf[13] = 0;						// Use default value of TO
	}

	memcpy(&(buf[14]), &packet, sizeof(packet));
	
	APIFrame frame(buf, 14+sizeof(packet));
	return send(frame);
}
	
bool bb::XBee::available() {
	if(operationStatus_ != RES_OK) return false;
	if(isInATMode()) leaveATMode();
	return uart_->available();
}

String bb::XBee::receive() {
	if(operationStatus_ != RES_OK) return "";
	if(isInATMode()) leaveATMode();

	String retval;
	while(uart_->available()) {
		retval += (char)uart_->read();
	}
	return retval;
}

bb::Result bb::XBee::receiveAndHandlePacket() {
	if(!packetMode_) {
		bb::Console::console.printfBroadcast("Wrong mode.\n");
		return RES_SUBSYS_WRONG_MODE;
	} 

	while(available()) {
		packetBuf_[packetBufPos_] = uart_->read();
		if(packetBuf_[packetBufPos_] & 0x80) {
			if(packetBufPos_ == sizeof(PacketFrame)-1) {
				PacketFrame frame;
				memcpy(&frame, packetBuf_, sizeof(PacketFrame));

				memset(packetBuf_, 0, sizeof(packetBuf_));
				packetBufPos_ = 0;

				if(frame.crc != calculateCRC(frame.packet)) {
					bb::Console::console.printfBroadcast("CRC incorrect - 0x%x instead of 0x%x\n", frame.crc, calculateCRC(frame.packet));
					return RES_PACKET_INVALID_PACKET;
				} 

				for(size_t i=0; i<receivers_.size(); i++) {
					receivers_[i]->incomingPacket(0, 0, frame.packet);
				}

				return RES_OK;
			} else {
				Console::console.printfBroadcast("Wrong packet size: %d bytes instead of %d\n", packetBufPos_+1, sizeof(PacketFrame));
				for(size_t i=0; i<=packetBufPos_; i++) {
					Console::console.printfBroadcast("0x%x ", packetBuf_[i]);
				}
				Console::console.printfBroadcast("\n");
				memset(packetBuf_, 0, sizeof(packetBuf_));
				packetBufPos_ = 0;
				return RES_PACKET_TOO_SHORT;
			}
		} else {
			packetBufPos_++;
			if(packetBufPos_ >= sizeof(packetBuf_)) {
				bb::Console::console.printfBroadcast("Packet too long: %d bytes instead of %d\n", packetBufPos_+1, sizeof(PacketFrame));

				memset(packetBuf_, 0, sizeof(packetBuf_));
				packetBufPos_ = 0;

				return RES_PACKET_TOO_LONG;
			}
		}
	}

	return RES_PACKET_TOO_SHORT;
}

bb::Result bb::XBee::receiveAndHandleAPIMode() {
	if(!apiMode_) {
		bb::Console::console.printfBroadcast("Wrong mode.\n");
		return RES_SUBSYS_WRONG_MODE;
	} 

	APIFrame frame;

	Result retval = receive(frame);
	if(retval != RES_OK) return retval;

#if 0
	Console::console.printfBroadcast("Received packet of length %d: ", frame.length());
	for(uint16_t i=0; i<frame.length(); i++) {
		Console::console.printfBroadcast("%x ", frame.data()[i]);
	}
	Console::console.printfBroadcast("\n");
#endif

	if(frame.data()[0] == 0x81) { // 16bit address frame
		if(frame.length() != sizeof(bb::Packet) + 5) {
			Console::console.printfBroadcast("Invalid API Mode packet size %d (expected %d)\n", frame.length(), sizeof(bb::Packet) + 5);
			return RES_SUBSYS_COMM_ERROR;
		}
		uint16_t source = (frame.data()[1] << 8) | frame.data()[2];
		uint8_t rssi = frame.data()[3];
		uint8_t options = frame.data()[4];

		bb::Packet packet;
		memcpy(&packet, &(frame.data()[5]), sizeof(packet));

//		Console::console.printfBroadcast("Sending packet from 0x%x (RSSI %d, options 0x%x) to receivers.\n", source, rssi, options);

		for(auto& r: receivers_) {
			r->incomingPacket(source, rssi, packet);
		}
	} else {
		Console::console.printfBroadcast("Unknown frame type 0x%x\n", frame.data()[0]);
	}

	return RES_OK;
}

String bb::XBee::sendStringAndWaitForResponse(const String& str, int predelay, bool cr) {
  	if(debug_ & DEBUG_XBEE_COMM) {
    	Console::console.printfBroadcast("Sending \"%s\"...", str.c_str());
  	}

    uart_->print(str);
  	if(cr) {
  		uart_->print("\r");
  	}
    uart_->flush();


    if(predelay > 0) delay(predelay);

    String retval;
    if(readString(retval)) {
    	if(debug_ & DEBUG_XBEE_COMM) {
    		Console::console.printfBroadcast(retval.c_str());
    		Console::console.printfBroadcast(" ");
    	}
    	return retval;
    }

    if(debug_ & DEBUG_PROTOCOL) {
    	Console::console.printfBroadcast("Nothing.\n");
    }
    return "";
}
  
bool bb::XBee::sendStringAndWaitForOK(const String& str, int predelay, bool cr) {
  	String result = sendStringAndWaitForResponse(str, predelay, cr);
  	if(result.equals("OK\r")) return true;
      
  	if(debug_ & DEBUG_PROTOCOL) {
    	Console::console.printfBroadcast("Expected \"OK\", got \"%s\"... \n", result.c_str());
  	}
  return false;
}

bool bb::XBee::readString(String& str, unsigned char terminator) {
	while(true) {
		int to = 0;
		while(!uart_->available()) {
			delay(1);
			to++;
			if(debug_ & DEBUG_XBEE_COMM) Console::console.printfBroadcast(".");
			if(to >= timeout_) {
				if(debug_ & DEBUG_PROTOCOL) Console::console.printfBroadcast("Timeout!\n");
				return false;
			}
		}
		unsigned char c = (unsigned char)uart_->read();
		str += (char)c;
		if(c == terminator) return true;
	}
}

bb::Result bb::XBee::sendAPIModeATCommand(uint8_t frameID, const char* cmd, uint8_t argument) {
	if(apiMode_ == false) return RES_CMD_INVALID_ARGUMENT;
	if(strlen(cmd) != 2) return RES_CMD_INVALID_ARGUMENT;

	uint8_t buf[5];
	buf[0] = 0x08; // AT command
	buf[1] = frameID;
	buf[2] = cmd[0];
	buf[3] = cmd[1];
	buf[4] = argument;

	APIFrame frame(buf, 5);
	if(send(frame) != RES_OK) return RES_SUBSYS_COMM_ERROR;

	bool received = false;
	for(int i=0; i<10 && received == false; i++) {
		if(uart_->available()) {
			if(receive(frame) != RES_OK) return RES_SUBSYS_COMM_ERROR;
			else received = true;
		} else delay(1);
	}
	if(!received) {
		Console::console.printfBroadcast("Timed out waiting for frame reply\n");
		return RES_SUBSYS_COMM_ERROR;
	}

	uint16_t length = frame.length();
	const uint8_t *data = frame.data();

	if(length < 5) return RES_SUBSYS_COMM_ERROR;
	if(data[0] != 0x88 || data[1] != frameID || data[2] != cmd[0] || data[3] != cmd[1]) return RES_SUBSYS_COMM_ERROR;
	switch(data[4]) {
	case 0:
		Console::console.printfBroadcast("OK.");
		return RES_OK;
		break;
	case 1:
		Console::console.printfBroadcast("XBee says ERROR.");
		return RES_SUBSYS_COMM_ERROR;
		break;
	case 2:
		Console::console.printfBroadcast("XBee says Invalid Command.");
		return RES_SUBSYS_COMM_ERROR;
		break;
	case 3:
		Console::console.printfBroadcast("XBee says Invalid Parameter.");
		return RES_SUBSYS_COMM_ERROR;
		break;
	default:
		break;
	}

	Console::console.printfBroadcast("XBee gives unknown error code.");
	return RES_SUBSYS_COMM_ERROR;
}

bb::XBee::APIFrame::APIFrame() {
	data_ = NULL;
	length_ = 0;
	calcChecksum();
}

bb::XBee::APIFrame::APIFrame(const uint8_t *data, uint16_t length) {
	data_ = new uint8_t[length];
	length_ = length;
	memcpy(data_, data, length);
	calcChecksum();
}

bb::XBee::APIFrame::APIFrame(bb::XBee::APIFrame& other) {
	if(other.data_ != NULL) {
		length_ = other.length_;
		data_ = new uint8_t[length_];
		memcpy(data_, other.data_, length_);
	} else {
		data_ = NULL;
		length_ = 0;
	}
	checksum_ = other.checksum_;
}

bb::XBee::APIFrame::APIFrame(uint16_t length) {
	data_ = new uint8_t[length];
	length_ = length;
}

bb::XBee::APIFrame& bb::XBee::APIFrame::operator=(const APIFrame& other) {
	if(other.data_ == NULL) {           	// they don't have data
		if(data_ != NULL) delete data_; 	// ...but we do - delete
		data_ = NULL;						// now we don't have data either
		length_ = 0;
	} else {  								// they do have data
		if(data_ != NULL) {					// and so do we
			if(length_ != other.length_) {	// but it's of a different size
				delete data_;				// reallocate
				length_ = other.length_;
				data_ = new uint8_t[length_];
			}
			memcpy(data_, other.data_, length_); // and copy
		} else {							// and we don't
			length_ = other.length_;		
			data_ = new uint8_t[length_];
			memcpy(data_, other.data_, length_);
		}
	}
	checksum_ = other.checksum_;

	return *this;
}

bb::XBee::APIFrame::~APIFrame() {
	if(data_ != NULL)
		delete data_;
}

void bb::XBee::APIFrame::calcChecksum() {
	checksum_ = 0;
	if(data_ != NULL) {
		for(uint16_t i=0; i<length_; i++) {
			checksum_ += data_[i];
		}
	}

	checksum_ = 0xff - (checksum_ & 0xff);
}

bb::XBee::APIFrame bb::XBee::APIFrame::atRequest(uint8_t frameID, uint16_t command) {
	APIFrame frame(4);

	frame.data_[0] = ATREQUEST; // local AT request
	frame.data_[1] = frameID;
	frame.data_[2] = (command>>8) & 0xff;
	frame.data_[3] = command & 0xff;
	frame.calcChecksum();

	return frame;
}

bool bb::XBee::APIFrame::isATRequest() {
	return data_[0] == ATREQUEST && length_ > 4;
}

bb::Result bb::XBee::APIFrame::unpackATResponse(uint8_t &frameID, uint16_t &command, uint8_t &status, uint8_t** data, uint16_t &length) {
	if(data_[0] != ATRESPONSE || length_ < 5) return RES_SUBSYS_COMM_ERROR;

	frameID = data_[1];
	command = (data_[2]<<8)|data_[3];
	status = data_[4];

	if(length_ > 5) {
		*data = &(data_[5]);
		length = length_-5;
	} else {
		*data = NULL;
		length = 0;
	}

	return RES_OK;
}


static inline int writeEscapedByte(HardwareSerial* uart, uint8_t byte) {
	int sent = 0;
	if(byte == 0x7d || byte == 0x7e || byte == 0x11 || byte == 0x13) {
		sent += uart->write(0x7d);
		sent += uart->write(byte ^ 0x20);
	} else {
		sent += uart->write(byte);
	}
	return sent;
}

static inline uint8_t readEscapedByte(HardwareSerial* uart) {
	uint8_t byte = uart->read();
	if(byte != 0x7d) return byte;
	return (uart->read()^0x20);
}

bb::Result bb::XBee::send(const APIFrame& frame) {
	if(apiMode_ == false) return RES_SUBSYS_WRONG_MODE;
	
	uint16_t length = frame.length();
	const uint8_t *data = frame.data();

	uint8_t lengthMSB = (length >> 8) & 0xff;
	uint8_t lengthLSB = length & 0xff;
	
	uart_->write(0x7e); // start delimiter
	writeEscapedByte(uart_, lengthMSB);
	writeEscapedByte(uart_, lengthLSB);

//	Console::console.printfBroadcast("Writing %d bytes: ", length);
	for(uint16_t i=0; i<length; i++) {
//		Console::console.printfBroadcast("%x ", data[i]);
		writeEscapedByte(uart_, data[i]);
	}
//	Console::console.printfBroadcast("\n");
//	Console::console.printfBroadcast("Writing checksum %x\n", frame.checksum());
	writeEscapedByte(uart_, frame.checksum());

	return RES_OK;
}

bb::Result bb::XBee::receive(APIFrame& frame) {
	uint8_t byte = 0xff;
	while(uart_->available()) {
		byte = uart_->read();
		if(byte == 0x7e) {
			break;
		} else {
			Console::console.printfBroadcast("Read garbage '%c' 0x%x\n", byte, byte);
		}
	}
	if(byte != 0x7e) {
		Console::console.printfBroadcast("Timed out waiting for start delimiter\n");
		return RES_SUBSYS_COMM_ERROR;
	}

	if(!uart_->available()) delay(1);
	uint8_t lengthMSB = readEscapedByte(uart_);
	if(!uart_->available()) delay(1);
	uint8_t lengthLSB = readEscapedByte(uart_);

	uint16_t length = (lengthMSB << 8) | lengthLSB;

	uint8_t *buf = new uint8_t[length];
	for(uint16_t i=0; i<length; i++) {
		if(!uart_->available()) delay(1);
		buf[i] = readEscapedByte(uart_);
	}

#if 0
	Console::console.printfBroadcast("Read %d bytes: ", length);
	for(uint16_t i=0; i<length; i++) {
		Console::console.printfBroadcast("%x ", buf[i]);
	}
	Console::console.printfBroadcast("\n");
#endif

	if(!uart_->available()) delay(1);
	uint8_t checksum = readEscapedByte(uart_);

	frame = APIFrame(buf, length);
	delete buf;
	if(frame.checksum() != checksum) {
		Console::console.printfBroadcast("Checksum invalid - expected 0x%x, got 0x%x\n", frame.checksum(), checksum);
		return RES_SUBSYS_COMM_ERROR;
	}

	return RES_OK;
}