#include "Arduino.h"
#include "main.hpp"

Preferences preference;

int32_t target_duty_ch[2] = {50, 50};
int32_t current_duty_ch[2] = {0, 0};
int32_t current_temperture = 4000;
const int32_t MAX_DUTY = pow(2, PWM_RES); // Max num: 512
const int32_t MINIMUM_DUTY = 10;          // Max num: 512

void init_led_driver()
{
    if (!ledcSetClockSource(LEDC_USE_XTAL_CLK))
    {
        Serial.println("LEDC set clock error!");
        return;
    }
    ledcAttach(LED_CH0_PIN, PWM_FREQ, PWM_RES);
    ledcAttach(LED_CH1_PIN, PWM_FREQ, PWM_RES);

    ledcWrite(LED_CH0_PIN, 0);
    ledcWrite(LED_CH1_PIN, 0);
}

void led_set_duty(int32_t *target_duty_array)
{
    Serial.print("set duty ch0 ch1: ");
    Serial.println(target_duty_array[0]);
    Serial.println(target_duty_array[1]);
    int32_t* max_duty_pt = target_duty_array[0] >= target_duty_array[1] ? &target_duty_array[0] : &target_duty_array[1];
    int32_t* minimum_duty_pt = target_duty_array[0] <= target_duty_array[1] ? &target_duty_array[0] : &target_duty_array[1];

    // Duty range limit
    if(*max_duty_pt > MAX_DUTY) {
        *max_duty_pt = MAX_DUTY;
    }
    if(*minimum_duty_pt < MINIMUM_DUTY) {
        *minimum_duty_pt = 0;
    }

    // LED CH0 5000k
    ledcFade(
        LED_CH0_PIN,
        uint32_t(current_duty_ch[0]),
        uint32_t(target_duty_array[0]),
        abs(target_duty_array[0]-current_duty_ch[0]) * LED_STEP_FADE_TIME_MS
    );
    // LED CH1 2700k
    ledcFade(
        LED_CH1_PIN,
        uint32_t(current_duty_ch[1]),
        uint32_t(target_duty_array[1]),
        abs(target_duty_array[1]-current_duty_ch[1]) * LED_STEP_FADE_TIME_MS
    );
    current_duty_ch[0] = target_duty_array[0];
    current_duty_ch[1] = target_duty_array[1];
}

inline void led_change_intensity_by_step(int32_t step_duty)
{
    int32_t target_duty_array[2] = {current_duty_ch[0] + step_duty, current_duty_ch[1] + step_duty};
    led_set_duty(target_duty_array);
}

inline void led_change_temperature(int32_t step_duty)
{
    int32_t target_duty_array[2] = {current_duty_ch[0] + step_duty, current_duty_ch[1] - step_duty};
    led_set_duty(target_duty_array);
}

inline void led_power_off()
{
    ledcFade(
        LED_CH0_PIN,
        uint32_t(current_duty_ch[0]),
        0,
        LED_POWER_ON_OFF_FADE_TIME_MS);
    // LED CH1 2700k
    ledcFade(
        LED_CH1_PIN,
        uint32_t(current_duty_ch[1]),
        0,
        LED_POWER_ON_OFF_FADE_TIME_MS);
}

inline void led_power_on()
{
    ledcFade(
        LED_CH0_PIN,
        0,
        uint32_t(current_duty_ch[0]),
        LED_POWER_ON_OFF_FADE_TIME_MS);
    // LED CH1 2700k
    ledcFade(
        LED_CH1_PIN,
        0,
        uint32_t(current_duty_ch[1]),
        LED_POWER_ON_OFF_FADE_TIME_MS);
}

/*********************ESP NOW**************************/
void readMacAddress()
{
    uint8_t baseMac[6];
    esp_err_t ret = esp_wifi_get_mac(WIFI_IF_STA, baseMac);
    if (ret == ESP_OK)
    {
        Serial.printf("%02x:%02x:%02x:%02x:%02x:%02x\n",
                      baseMac[0], baseMac[1], baseMac[2],
                      baseMac[3], baseMac[4], baseMac[5]);
    }
    else
    {
        Serial.println("Failed to read MAC address");
    }
}

void printMAC(const uint8_t *mac_addr)
{
    char macStr[18];
    snprintf(macStr, sizeof(macStr), "%02x:%02x:%02x:%02x:%02x:%02x",
             mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
    Serial.print(macStr);
}

bool addPeer(const uint8_t *peer_addr)
{
    memset(&slave, 0, sizeof(slave));
    const esp_now_peer_info_t *peer = &slave;
    memcpy(slave.peer_addr, peer_addr, 6);

    // slave.channel = channel; // pick a channel
    slave.channel = 4;
    slave.encrypt = false;       // no encryption

    // check if the peer exists
    bool exists = esp_now_is_peer_exist(slave.peer_addr);
    if (exists)
    {
        // Slave already paired.
        Serial.println("Already Paired");
        return true;
    }
    else
    {
        esp_err_t addStatus = esp_now_add_peer(peer);
        if (addStatus == ESP_OK)
        {
            // Pair success
            Serial.println("Pair success");
            return true;
        }
        else
        {
            Serial.println("Pair failed");
            return false;
        }
    }
}

// callback when data is sent
void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status)
{
    Serial.print("Last Packet Send Status: ");
    Serial.print(status == ESP_NOW_SEND_SUCCESS ? "Delivery Success to " : "Delivery Fail to ");
    printMAC(mac_addr);
    Serial.println();
}

void OnDataRecv(const uint8_t *mac_addr, const uint8_t *incomingData, int len)
{
    Serial.print(len);
    Serial.println(" bytes of new data received.");
    MessageType type = MessageType(incomingData[0]); // first message byte is the type of message
    switch (type)
    {
        case DATA: // the message is data type
        {
            memcpy(&incomingReadings, incomingData, sizeof(incomingReadings));
            if (incomingReadings.command == SET_DUTY)
            {
                led_set_duty(incomingReadings.data);
            }
            else if (incomingReadings.command == POWER_OFF)
            
            {
                led_power_off();
            }
            else if (incomingReadings.command == POWER_ON)
            {
                led_power_on();
            }

            outgoingSetpoints.msgType = DATA;
            outgoingSetpoints.id = BOARD_ID;
            outgoingSetpoints.command = incomingReadings.command;
            outgoingSetpoints.data[0] = current_duty_ch[0];
            outgoingSetpoints.data[1] = current_duty_ch[1];

            esp_err_t result = esp_now_send(clientMacAddress, (uint8_t *) &outgoingSetpoints, sizeof(outgoingSetpoints));
            if (result != ESP_OK) {
                Serial.printf("Send data fault, Error code: 0X%x", result);
            }
            break;  
        }
        case PAIRING:// the message is a pairing request
        {
            memcpy(&pairingData, incomingData, sizeof(pairingData));
            Serial.println(pairingData.msgType);
            Serial.println(pairingData.id);
            Serial.print("Pairing request from MAC Address: ");
            printMAC(pairingData.macAddr);
            Serial.print(" on channel ");
            Serial.println(pairingData.channel);

            memcpy(clientMacAddress, pairingData.macAddr, sizeof(clientMacAddress));

            if (pairingData.id > 0)
            { // do not replay to server itself
                if (pairingData.msgType == PAIRING)
                {
                    pairingData.id = 0; // 0 is server
                    // Server is in AP_STA mode: peers need to send data to server soft AP MAC address
                    esp_wifi_get_mac(WIFI_IF_STA, pairingData.macAddr);
                    Serial.print("Pairing MAC Address: ");
                    printMAC(clientMacAddress);
                    pairingData.channel = channel;
                    Serial.println(" send response");
                    // esp_err_t result = esp_now_send(clientMacAddress, (uint8_t *)&pairingData, sizeof(pairingData));
                    addPeer(clientMacAddress);
                }
            }
            break;
        }
    }
}

void init_esp_now()
{
    if (esp_now_init() != ESP_OK)
    {
        Serial.println("Error initializing ESP-NOW");
        return;
    }
    esp_now_register_recv_cb(esp_now_recv_cb_t(OnDataRecv));
    esp_now_register_send_cb(OnDataSent);
}


bool areMacEqual(const uint8_t mac1[], const uint8_t mac2[], int size) {
  if (mac1 == nullptr || mac2 == nullptr) {
    return false; // 或者根据具体需求选择其他处理方式
  }
  for (int i = 0; i < size; ++i) {
    if (mac1[i] != mac2[i]) {
      return false; // 发现不相等元素，立即返回false
    }
  }
  return true; // 循环结束，所有元素都相等
}


/********************Main Function************************/

void setup()
{
    Serial.begin(115200);

    bool format_test = true;
    if (format_test) { 
        preference.clear();
    }

    // WIFI initialize
    WiFi.mode(WIFI_STA);
    WiFi.STA.begin();
    Serial.print("Server MAC Address: ");
    readMacAddress();

    channel = WiFi.channel();
    Serial.print("Station IP Address: ");
    Serial.println(WiFi.localIP());
    Serial.print("Wi-Fi Channel: ");
    Serial.println(channel);

    // ESP NOW initialize
    init_esp_now();
    slave.channel = channel; // pick a channel
    slave.encrypt = false;       // no encryption
    memcpy(slave.peer_addr, clientMacAddress, sizeof(clientMacAddress));
    esp_err_t addStatus = esp_now_add_peer(&slave);
    if (addStatus != ESP_OK) {
        Serial.println("Failed to add peer");
    } else {
        Serial.println("Peer added successfully");
    }
    // addPeer(clientMacAddress);

    // LED driver initialize
    init_led_driver();

    // preference initialize
    /*
        preference.begin("my_preference", false);
    uint8_t zero_mac_address[6] = {0};
    preference.getBytes("peer_mac_address", clientMacAddress, sizeof(clientMacAddress));
    if(!areMacEqual(clientMacAddress, zero_mac_address, sizeof(clientMacAddress))) {
        if (!addPeer(clientMacAddress)) {
            Serial.println("Add peer error!");
            return;
        }
        
    }
    else {
        Serial.println("wait for pairing...");
    }
    */

    
}

void loop()
{
    
}
