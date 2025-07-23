#pragma once


#include <esp_now.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include <cmath>
#include <Preferences.h>



/*********LED Driver*********/
#define LED_CH0_PIN 0 // 5000k
#define LED_CH1_PIN 1 // 2700k
#define PWM_RES 9
#define PWM_FREQ 20000

#define LED_STEP_FADE_TIME_MS 3
#define LED_POWER_ON_OFF_FADE_TIME_MS 2000

#define MAX_TEMPERTURE 5000
#define MINIMUM_TEMPERTURE 2700


/*********************ESP NOW**************************/
#define BOARD_ID 10
esp_now_peer_info_t slave;
uint8_t channel;

enum MessageType
{
    PAIRING,
    DATA,
};
MessageType messageType;

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
uint8_t clientMacAddress[6] = {0x50, 0x40, 0x6f, 0x40, 0xba, 0x18};

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

struct_message incomingReadings;
struct_message outgoingSetpoints;
struct_pairing pairingData;