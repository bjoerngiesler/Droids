#if !defined(BBERROR_H)
#define BBERROR_H

#include <Arduino.h>

namespace bb {

typedef enum {
	ERROR_NOT_PRESENT = 0x00,
	ERROR_OK          = 0x01,
	ERROR_COMM        = 0x02,
	ERROR_FLAGS       = 0x0f,
	ERROR_VOLTAGE     = 0x10,
	ERROR_TEMP        = 0x20,
	ERROR_OVERLOAD    = 0x40,
	ERROR_OTHER       = 0x80
} ErrorState;

typedef enum {
	RES_OK = 0, // no error :-) 
	RES_PARAM_NO_SUCH_PARAMETER = 1,
	RES_PARAM_INVALID_TYPE = 2,
	RES_SUBSYS_HW_DEPENDENCY_MISSING = 3,
	RES_SUBSYS_HW_DEPENDENCY_LOCKED = 4,
	RES_SUBSYS_ALREADY_REGISTERED = 5,
	RES_SUBSYS_NO_SUCH_SUBSYS = 6,
	RES_SUBSYS_RESOURCE_NOT_AVAILABLE = 7,
	RES_SUBSYS_COMM_ERROR = 8,
	RES_SUBSYS_PROTOCOL_ERROR = 9,
	RES_SUBSYS_ALREADY_STARTED = 10,
	RES_SUBSYS_NOT_STARTED = 11,
	RES_SUBSYS_NOT_OPERATIONAL = 12,
	RES_SUBSYS_NOT_STOPPABLE = 13,
	RES_PARAM_INVALID_VALUE = 14,
	RES_PARAM_READONLY_PARAMETER = 15,
	RES_CONFIG_INVALID_HANDLE = 16,
	RES_SUBSYS_NOT_INITIALIZED = 17,
	RES_SUBSYS_ALREADY_INITIALIZED = 18,
	RES_COMM_TIMEOUT = 19,
	RES_CMD_UNKNOWN_COMMAND = 20,
	RES_CMD_INVALID_ARGUMENT_COUNT = 21,
	RES_CMD_INVALID_ARGUMENT = 22,
	RES_COMMON_NOT_IN_LIST = 23,
	RES_COMMON_DUPLICATE_IN_LIST = 24,
	RES_SUBSYS_WRONG_MODE = 25,
	RES_PACKET_TOO_SHORT = 26,
	RES_PACKET_TOO_LONG = 27,
	RES_PACKET_INVALID_PACKET = 28,
	RES_CMD_FAILURE = 29,
	RES_COMMON_OUT_OF_RANGE = 30
} Result;

const String& errorMessage(Result res);

};

#endif // BBERROR_H