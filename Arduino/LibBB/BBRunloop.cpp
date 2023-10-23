#include <Arduino.h>
#include <limits.h>
#include "BBRunloop.h"
#include "BBConsole.h"

bb::Runloop bb::Runloop::runloop;

bb::Runloop::Runloop() {
	name_ = "runloop";
	description_ = "Main runloop";
	help_ = "Started once after all subsystems are added. Its start() only returns if stop() is called.\n"\
"Commands:\n"\
"\trunning_status [on|off]: Print running status on timing";
	cycleTime_ = DEFAULT_CYCLETIME;
	runningStatus_ = false;
	excuseOverrun_ = false;
}

bb::Result bb::Runloop::start(ConsoleStream* stream) {
	(void)stream;
	running_ = true;
	started_ = true;
	operationStatus_ = RES_OK;
	seqnum_ = 0;
	startTime_ = millis();

	if(Console::console.isStarted()) Console::console.printGreeting();
	while(running_) {
		unsigned long micros_start_loop = micros();
		seqnum_++;

		// First of all run any timed callbacks...
		uint64_t m = millis();
		for(std::vector<TimedCallback>::iterator iter = timedCallbacks_.begin(); iter != timedCallbacks_.end(); iter++) {
			if(iter->triggerMS < m) { // FIXME there is highly likely an integer wrap in here...?
				iter->cb();
				if(true == iter->oneshot) iter = timedCallbacks_.erase(iter);
				else iter->triggerMS = m + iter->deltaMS;
			}
		}

		// ...then run step() on all subsystems...
		std::map<String, unsigned long> timingInfo;

		std::vector<Subsystem*> subsys = SubsystemManager::manager.subsystems();
		for(auto& s: subsys) {
			unsigned long us = micros();
			if(s->isStarted() && s->operationStatus() == RES_OK) {
				s->step();
			}

			timingInfo[s->name()] = micros()-us;
			if(runningStatus_) Console::console.printBroadcast(s->name() + ": " + (micros()-us) + "us ");
		}

		// ...find out how long we took...
		unsigned long micros_end_loop = micros();
		unsigned long looptime;
		if(micros_end_loop >= micros_start_loop)
			looptime = micros_end_loop - micros_start_loop;
		else
			looptime = ULONG_MAX - micros_start_loop + micros_end_loop;
		if(runningStatus_) Console::console.printlnBroadcast(String("Total: ") + looptime + "us");

		// ...and bicker if we overran the allotted time.
		if(looptime <= cycleTime_) {
			delayMicroseconds(cycleTime_-looptime);
		} else if(excuseOverrun_ == false) {
			Console::console.printBroadcast(String(looptime) + "us spent in loop: ");
			for(auto& t: timingInfo) {
				Console::console.printBroadcast(t.first + ": " + t.second + "us ");
			}
			Console::console.printlnBroadcast("");
		}

		excuseOverrun_ = false;
	}

	started_ = false;
	operationStatus_ = RES_SUBSYS_NOT_STARTED;
	return RES_OK;
}

bb::Result bb::Runloop::stop(ConsoleStream *stream) {
	stream = stream; // make compiler happy
	if(!started_) return RES_SUBSYS_NOT_STARTED;
	return RES_SUBSYS_NOT_STOPPABLE;
}

bb::Result bb::Runloop::step() {
	return RES_OK;
}

bb::Result bb::Runloop::handleConsoleCommand(const std::vector<String>& words, ConsoleStream *stream) {
	stream->println(String("Command: ") + words[0]);
	if(words[0] == "running_status") {
		if(words.size() != 2) return RES_CMD_INVALID_ARGUMENT_COUNT;
		runningStatus_ = words[1] == "on" ? true : false;
		return RES_OK;
	}

	return bb::Subsystem::handleConsoleCommand(words, stream);;
}

void bb::Runloop::setCycleTime(unsigned int t) {
 	cycleTime_ = t;
}

unsigned int bb::Runloop::cycleTime() {
	return cycleTime_;
}

void bb::Runloop::excuseOverrun() {
	excuseOverrun_ = true;
}

uint64_t bb::Runloop::millisSinceStart() {
	return millis() - startTime_;
}

void* bb::Runloop::scheduleTimedCallback(uint64_t ms, std::function<void(void)> cb, bool oneshot) {
	TimedCallback c = {millis() + ms, ms, oneshot, cb};
	timedCallbacks_.push_back(c);
	return (void*)&(*timedCallbacks_.end());
}

bb::Result bb::Runloop::cancelTimedCallback(void* handle) {
	for(std::vector<TimedCallback>::iterator iter = timedCallbacks_.begin(); iter != timedCallbacks_.end(); iter++) {
		if(handle == (void*)&(*iter)) {
			timedCallbacks_.erase(iter);
			return RES_OK;
		}
	}
	return RES_COMMON_NOT_IN_LIST;
}


