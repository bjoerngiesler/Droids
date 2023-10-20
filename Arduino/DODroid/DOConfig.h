#if !defined(DOCONFIG_H)
#define DOCONFIG_H

#include <Arduino.h>
#include <BBPacket.h>

static const bb::DroidType DROID_TYPE = bb::DroidType::DROID_DO;
static const char*         DROID_NAME = "Generic D-O";

// Network config
static const uint16_t COMMAND_UDP_PORT = 2000; // BB8 listens on this port for commands (see BB8Packet.h for command structure)
static const uint16_t STATE_UDP_PORT   = 2001; // BB8 sends running state on this port
static const uint16_t REPLY_UDP_PORT   = 2002; // This port is used to reply to special commands

// Left side pins
static const uint8_t PULL_DOWN_A0      = 15; // A0
static const uint8_t P_RIGHT_ENCB      = 16; // A1
static const uint8_t P_RIGHT_ENCA      = 17; // A2
static const uint8_t P_LEFT_PWMA       = 18; // A3
static const uint8_t P_LEFT_PWMB       = 19; // A4
static const uint8_t UNUSED4           = 20; // A5
static const uint8_t P_DYNAMIXEL_RTS   = 21; // A6
static const uint8_t P_SERIALTX_TX     = 0;  // OK
static const uint8_t P_SERIALTX_RX     = 1;  // OK
static const uint8_t P_RIGHT_PWMA      = 2;
static const uint8_t P_RIGHT_PWMB      = 3;
static const uint8_t P_STATUS_NEOPIXEL = 4;
static const uint8_t P_BALL1_NEOPIXEL  = 5;

// Right side pins
static const uint8_t P_LEFT_ENCA       = 6;   // OK
static const uint8_t P_LEFT_ENCB       = 7;   // OK
static const uint8_t P_DFPLAYER_TX     = 8;   // OK
static const uint8_t P_DFPLAYER_RX     = 9;   // OK
static const uint8_t P_BALL2_NEOPIXEL  = 10;
static const uint8_t P_I2C_SDA         = 11;  // OK
static const uint8_t P_I2C_SCL         = 12;  // OK
static const uint8_t P_DYNAMIXEL_RX    = 13;
static const uint8_t P_DYNAMIXEL_TX    = 14;

static const float WHEEL_CIRCUMFERENCE = 722.566310325652445;
static const float WHEEL_TICKS_PER_TURN = 979.2 * (96.0/18.0); // 979 ticks per one turn of the drive gear, 18 teeth on the drive gear, 96 teeth on the main gear.

static const uint8_t BATT_STATUS_ADDR   = 0x40;

#endif // DOCONFIG_H