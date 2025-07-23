#pragma once

#include "AiEsp32RotaryEncoder.h"
#include "Button2.h"
#include <WiFi.h>
#include <esp_wifi.h>
#include <esp_now.h>
#include <cmath>
#include <driver/rtc_io.h>
#include <Preferences.h>


#define ROTARY_ENCODER_A_PIN 2
#define ROTARY_ENCODER_B_PIN 3
#define ROTARY_ENCODER_STEPS 4


#define ENCODER_BUTTON_PIN 4
#define BTN_DEBOUNCE_MS 50
#define BTN_LONGCLICK_MS 200
#define BTN_DOUBLECLICK_MS 300

#define LED2_PIN 5

#define PWM_RES 9    
#define MAX_RETRY_COUNT 3
#define TEMPERTURE_CHANGE_WEIGHT 5
#define LUMINANCE_CHANGE_WEIGHT 1

#define BUTTON_PIN_BITMASK(GPIO) (1ULL << GPIO) 

enum LedState
{
    LED_POWER_OFF = 0,
    LED_POWER_ON = 1,
};
LedState ledState = LED_POWER_OFF;


/**********ESP NOW********* */
#define BOARD_ID 1
#define MAX_CHANNEL 13 // 11 in North America or 13 in China

unsigned long currentMillis = millis();
unsigned long previousMillis = 0; // Stores last time temperature was published
const long interval = 10000;      // Interval at which to publish sensor readings
unsigned long start_time = 0;              // used to measure Pairing time
unsigned int readingId = 0;

uint8_t serverAddress[] = {0x80, 0x64, 0x6f, 0x41, 0x5d, 0x5c};
uint8_t full_mac_address[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
uint8_t clientMacAddress[6];

int16_t rotary_direction;


enum MessageType
{
    PAIRING,
    DATA,
};
MessageType messageType;

enum PairingStatus
{
    NOT_PAIRED,
    PAIR_REQUEST,
    PAIR_REQUESTED,
    PAIR_PAIRED,
    HAVE_RECORD,
};
PairingStatus pairingStatus = PAIR_REQUEST;

enum MessageCommand
{
    SET_DUTY,
    SET_TEMPERATURE,
    RESPONSE_SET_DUTY,
    POWER_ON,
    POWER_OFF,
};
MessageCommand Messagecommand;

uint16_t counter = 0;

// Set received data structure
struct struct_message
{
    uint8_t msgType;
    uint8_t id;
    MessageCommand command;
    int32_t data[2];
};

// set pairing structure
struct struct_pairing
{
    uint8_t msgType;
    uint8_t id;
    uint8_t macAddr[6];
    uint8_t channel;
};

esp_now_peer_info_t peer_info;

struct_message myData;  // data to send
struct_message inData;  // data received
struct_pairing pairingData;

#ifdef SAVE_CHANNEL
int lastChannel;
#endif
int channel = 0;


uint16_t pairing_retry_counter = 0;
uint16_t server_board_id = 10;

void saveMyPreference(const char* name, const char* key, const void* value, size_t len);
