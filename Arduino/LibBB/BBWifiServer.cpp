#include <BBWifiServer.h>
#include <BBRunloop.h>
#if !defined(ARDUINO_PICO_VERSION_STR)
#include <ArduinoOTA.h>
#endif

String IPAddressToString(const IPAddress& addr) {
	return String(addr[0]) + "." + addr[1] + "." + addr[2] + "." + addr[3];
}

bb::WifiServer bb::WifiServer::server;


bb::WiFiConsoleStream::WiFiConsoleStream() {
}

void bb::WiFiConsoleStream::setClient(const WiFiClient& client) {
	client_ = client;
}

bool bb::WiFiConsoleStream::available() {
	return client_.available();
}

bool bb::WiFiConsoleStream::readStringUntil(unsigned char c, String& str) {
	int timeout = 0;
	while(true) {
		if(client_.available()) {
			unsigned char data = client_.read();
			str += (char)data;
			if(data == c) return true;
		} else {
			delay(1);
			timeout++;
			if(timeout >= 1000) return false;
		}
	}
}

void bb::WiFiConsoleStream::print(size_t val) {
	client_.print(val);
	client_.flush();
}

void bb::WiFiConsoleStream::print(int val) {
	client_.print(val);
	client_.flush();
}

void bb::WiFiConsoleStream::print(float val) {
	client_.print(val);
	client_.flush();
}

void bb::WiFiConsoleStream::print(const String& val) {
	client_.print(val);
	client_.flush();
}

void bb::WiFiConsoleStream::println(int val) {
	client_.println(val);
	client_.flush();
}

void bb::WiFiConsoleStream::println(float val) { 
	client_.println(val);
	client_.flush();
}

void bb::WiFiConsoleStream::println(const String& val) {
	client_.println(val);
	client_.flush();
}

void bb::WiFiConsoleStream::println() {
	client_.println();
	client_.flush();
}

bb::WifiServer::WifiServer() {
	tcp_ = NULL;
	udpStarted_ = false;

	name_ = "wifi";
	help_ = "Creates an access point or joins a local network. Starts a TCP server for interactive configuration and a UDP server for remote control.\r\nSSID and WPA Key replacements: $MAC - Mac address (hex format, no colons)";
	description_ = "Wifi communication module (uninitialized)";
	
	parameters_.push_back({"ssid", PARAMETER_STRING, "SSID for the Wifi server (if it contains spaces enclose in \"\")"});
 	parameters_.push_back({"wpa_key", PARAMETER_STRING, "Password for the Wifi server (if it contains spaces enclose in \"\")"});
	parameters_.push_back({"ap", PARAMETER_UINT, "Create an access point (1) or try to connect to an existing network (0)"});
	parameters_.push_back({"terminal_port", PARAMETER_UINT, "TCP port for the terminal server"});
	parameters_.push_back({"remote_port", PARAMETER_UINT, "UDP port for remote control"});
}

bb::Result bb::WifiServer::initialize(const String& ssid, const String& wpakey, bool apmode, uint16_t udpPort, uint16_t tcpPort) {
	if(operationStatus_ != RES_SUBSYS_NOT_INITIALIZED) return RES_SUBSYS_ALREADY_INITIALIZED;

	paramsHandle_ = ConfigStorage::storage.reserveBlock(sizeof(params_));
	Serial.print("Handle: "); Serial.println(paramsHandle_);
	if(ConfigStorage::storage.blockIsValid(paramsHandle_)) {
		Serial.println("Block valid, reading config");
		ConfigStorage::storage.readBlock(paramsHandle_, (uint8_t*)&params_);
	} else {
		Serial.println("Block invalid, initializing");
		memset(&params_, 0, sizeof(params_));
		strncpy(params_.ssid, ssid.c_str(), MAX_STRLEN);
		strncpy(params_.wpaKey, wpakey.c_str(), MAX_STRLEN);
		params_.ap = apmode;
		params_.udpPort = udpPort;
		params_.tcpPort = tcpPort;
	}

	setOTANameAndPassword(DEFAULT_SSID, DEFAULT_WPAKEY);

	operationStatus_ = RES_SUBSYS_NOT_STARTED;
	return Subsystem::initialize();
}

bb::Result bb::WifiServer::setOTANameAndPassword(const String& name, const String& password) {
#if !defined(ARDUINO_PICO_VERSION_STR)
	if(WiFi.status() == WL_NO_MODULE) return RES_SUBSYS_RESOURCE_NOT_AVAILABLE;

	byte mac[6];
	WiFi.macAddress(mac);

	char tmp[18]; 
	memset(tmp, 0, sizeof(tmp));
	sprintf(tmp, "%02x:%02x:%02x:%02x:%02x:%02x", mac[5], mac[4], mac[3], mac[2], mac[1], mac[0]);

	otaName_ = name;
	otaName_.replace("$MAC", tmp);
	otaPassword_ = password;
	otaPassword_.replace("$MAC", tmp);

	return RES_OK;
#else
	return RES_SUBSYS_RESOURCE_NOT_AVAILABLE;
#endif
}


bb::WifiServer::~WifiServer() {
}

bb::Result bb::WifiServer::start(ConsoleStream* stream) {
	if(WiFi.status() == WL_NO_MODULE) return RES_SUBSYS_RESOURCE_NOT_AVAILABLE;

	byte mac[6];
	WiFi.macAddress(mac);

	char tmp[18]; 
	memset(tmp, 0, sizeof(tmp));
	sprintf(tmp, "%02x:%02x:%02x:%02x:%02x:%02x", mac[5], mac[4], mac[3], mac[2], mac[1], mac[0]);
	macStr_ = tmp;

	ssid_ = params_.ssid;
	wpaKey_ = params_.wpaKey;
	ssid_.replace("$MAC", macStr_);
	wpaKey_.replace("$MAC", macStr_);

	if(params_.ap == true) {
		if(stream) {
			stream->print(ssid_); 
			stream->print("\"...");
		}

		uint8_t retval = WiFi.beginAP(ssid_.c_str(), wpaKey_.c_str(), random(1, 15));
		if(retval != WL_AP_LISTENING) return RES_SUBSYS_RESOURCE_NOT_AVAILABLE;
	} else {
		if(stream) {
			stream->print("Trying to connect to SSID \""); 
			stream->print(ssid_); 
			stream->print("\"... ");
		}
		uint8_t retval = WiFi.begin(ssid_.c_str(), wpaKey_.c_str());
		if(retval != WL_CONNECTED) return RES_SUBSYS_RESOURCE_NOT_AVAILABLE;

		if(stream) stream->print("trying to start TCP Server... ");
		if(startTCPServer(stream)) {
			if(stream) stream->print("success. ");
		} else if(stream) stream->print("failure. ");
	}

	if(stream) stream->print("trying to start UDP Server... ");
	if(startUDPServer(stream)) {
		if(stream) stream->print("success. ");
	} else if(stream) stream->print("failure. ");

	operationStatus_ = RES_OK;
	started_ = true;

	return RES_OK;
}

bb::Result bb::WifiServer::stop(ConsoleStream* stream) {
	if(stream) stream = stream; // make compiler happy

	if(client_) {
		client_.stop();
		bb::Console::console.removeConsoleStream(&consoleStream_);
	}

	if(tcp_) {
		delete tcp_;
		tcp_ = NULL;
	}

	if(udpStarted_) udp_.stop();
	udpStarted_ = false;
	WiFi.end();

	started_ = false;
	operationStatus_ = RES_SUBSYS_NOT_STARTED;
	return RES_OK;
}

bb::Result bb::WifiServer::step() {
	int status = WiFi.status();
	static int seqnum = 0;

	if(seqnum == 1e6/Runloop::runloop.cycleTime()) {
#if !defined(ARDUINO_PICO_VERSION_STR)
		//Console::console.printlnBroadcast("Polling OTA");
		ArduinoOTA.poll();
#endif
		seqnum = 0;
		return RES_OK;
	}

	seqnum++;

	if(tcp_ != NULL && status != WL_AP_CONNECTED && status != WL_CONNECTED) {
		delete tcp_;
		tcp_ = NULL;
		return RES_OK;
	} 

	if(tcp_ == NULL && (status == WL_AP_CONNECTED || status == WL_CONNECTED)) {
		Console::console.printlnBroadcast("Starting TCP server");
		startTCPServer();
	}

	if(tcp_ != NULL) {
		if(client_ == false) {
			client_ = tcp_->available();

			if(client_ == true) {
				Console::console.printlnBroadcast("New client connected");
				consoleStream_.setClient(client_);
				bb::Console::console.addConsoleStream(&consoleStream_);
			}
		} else if(client_.connected() == false || client_.status() == 0) {
			Console::console.printlnBroadcast("Client disconnected");
			client_.stop();
			bb::Console::console.removeConsoleStream(&consoleStream_);
		}
	}

	return RES_OK;
}

bb::Result bb::WifiServer::parameterValue(const String& name, String& value) {
	if(name == "ssid") { 
		value = params_.ssid; return RES_OK; 
	} else if(name == "wpa_key") {
		value = params_.wpaKey; return RES_OK;
	} else if(name == "ap") {
		if(params_.ap) value = "1"; else value = "0";
		return RES_OK;
	} else if(name == "terminal_port") {
		value = String(params_.tcpPort);
		return RES_OK;
	} else if(name == "remote_port") {
		value = String(params_.udpPort);
		return RES_OK;
	} 

	return RES_PARAM_NO_SUCH_PARAMETER;
}

bb::Result bb::WifiServer::setParameterValue(const String& name, const String& value) {
	Result res = RES_PARAM_NO_SUCH_PARAMETER;

	if(name == "ssid") { 
		strncpy(params_.ssid, value.c_str(), MAX_STRLEN);
		res = RES_OK;
	} else if(name == "wpa_key") {
		strncpy(params_.wpaKey, value.c_str(), MAX_STRLEN);
		res = RES_OK;
	} else if(name == "ap") {
		if(value == "1") { params_.ap = true; res = RES_OK; }
		else if(value == "0") { params_.ap = false; res = RES_OK; }
		else res = RES_PARAM_INVALID_TYPE;
	} else if(name == "terminal_port") {
		int v = value.toInt();
		if(v < 0 || v > 65536) return RES_PARAM_INVALID_VALUE;
		params_.tcpPort = v;
		res = RES_OK;
	} else if(name == "remote_port") {
		int v = value.toInt();
		if(v < 0 || v > 65536) return RES_PARAM_INVALID_VALUE;
		params_.udpPort = v;
		res = RES_OK;
	} 

	if(res == RES_OK) ConfigStorage::storage.writeBlock(paramsHandle_, (uint8_t*)&params_);

	return res;
}

bb::Result bb::WifiServer::operationStatus() {
	if(!started_) return RES_SUBSYS_NOT_STARTED;
	if(udpStarted_) return RES_OK;
	return RES_SUBSYS_RESOURCE_NOT_AVAILABLE;
}

bool bb::WifiServer::isAPStarted() {
  return WiFi.status() == WL_AP_LISTENING || WiFi.status() == WL_AP_CONNECTED;
}

bool bb::WifiServer::isConnected() {
  return WiFi.status() == WL_CONNECTED;
}

bool bb::WifiServer::startUDPServer(ConsoleStream *stream) {
	(void)stream;

	if(udp_.begin(params_.udpPort)) {
		udpStarted_ = true;
		return true;
	}
	return false;
}

bool bb::WifiServer::startTCPServer(ConsoleStream *stream) {
	if(tcp_ != NULL) // already started
		return false;
	tcp_ = new WiFiServer(params_.tcpPort);
	tcp_->begin();
#if !defined(ARDUINO_PICO_VERSION_STR)
	if(stream != NULL) stream->print("starting OTA... ");	
	ArduinoOTA.begin(WiFi.localIP(), otaName_.c_str(), otaPassword_.c_str(), InternalStorage);
#endif

	return true;
}

bool bb::WifiServer::broadcastUDPPacket(const uint8_t* packet, size_t len) {
	IPAddress ip = WiFi.localIP();
	ip[3] = 0xff;

	return sendUDPPacket(ip, packet, len);
}

bool bb::WifiServer::sendUDPPacket(const IPAddress& addr, const uint8_t* packet, size_t len) {
	if(WiFi.status() != WL_CONNECTED && WiFi.status() != WL_AP_CONNECTED) return false;
	if(!udpStarted_) return false;
	if(udp_.beginPacket(addr, params_.udpPort) == false) {
		Console::console.printlnBroadcast("WiFiUDP.beginPacket() failed!");
		return false;
	}

	if(udp_.write(packet, len) != len) {
		Console::console.printlnBroadcast("WiFiUDP.write() failed");
		return false;
	}

	if(udp_.endPacket() == false) {
		Console::console.printlnBroadcast("WiFiUDP.endPacket() failed");
	}

	return true;
}


unsigned int bb::WifiServer::readDataIfAvailable(uint8_t *buf, unsigned int maxsize, IPAddress& remoteIP) {
	unsigned int len = udp_.parsePacket();
	if(!len) return 0;
	remoteIP = udp_.remoteIP();
	if(len > maxsize) return len;
	if((unsigned int)(udp_.read(buf, maxsize)) != len) { 
		Serial.println("Huh? Differing sizes?!"); 
		return 0;
	} else {
		return len;
	}
}

void bb::WifiServer::updateDescription() {
	if(WiFi.status() == WL_NO_MODULE) {
		description_ = "Wifi communication module, status: no Wifi module installed.";
		return;
	}

	description_ = String("Wifi module ") + macStr_ + ", status: ";
	
	IPAddress ip;

	switch(WiFi.status()) {
    case WL_IDLE_STATUS:
    	description_ += "idle";
    	break;
    case WL_NO_SSID_AVAIL:
    	description_ += "no SSID available";
    	break;
    case WL_SCAN_COMPLETED:
    	description_ += "scan completed";
    	break;
    case WL_CONNECTED:
    	description_ += "connected as client, local IP ";
    	ip = WiFi.localIP();
    	description_ += String(ip[0]) + "." + ip[1] + "." + ip[2] + "." + ip[3];
    	break;
    case WL_CONNECT_FAILED:
    	description_ += "connection failed";
    	break;
    case WL_CONNECTION_LOST:
    	description_ += "connection lost";
    	break;
    case WL_DISCONNECTED:
    	description_ += "disconnected";
    	break;
    case WL_AP_LISTENING:
    	description_ += "AP listening";
    	break;
    case WL_AP_CONNECTED:
    	description_ += "connected as AP, local IP " + IPAddressToString(WiFi.localIP());
    	//ip = WiFi.localIP();
    	//description_ += String(ip[0]) + "." + ip[1] + "." + ip[2] + "." + ip[3];
    	break;
    case WL_AP_FAILED:
    	description_ += "AP setup failed";
    	break;
    default:
    	description_ += "unknown status";
    	break;
	}

	if(client_ == true) {
		description_ += ", client ";
		ip = client_.remoteIP();
  	description_ += ip[0];
  	description_ += ".";
  	description_ += ip[1];
  	description_ += ".";
  	description_ += ip[2];
  	description_ += ".";
  	description_ += ip[3];
		description_ += " connected";
	}
}