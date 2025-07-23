/*
ledc arduino: https://docs.espressif.com/projects/arduino-esp32/en/latest/api/ledc.html
led rtc config: https://www.espressif.com/sites/default/files/documentation/esp8684_technical_reference_manual_cn.pdf#ledpwm
*/

#include "Arduino.h"
#include "main.hpp"

Preferences preference;
Button2 encoderButton(ENCODER_BUTTON_PIN);

/*****************LED Controller*******************/

const uint16_t MAX_DUTY = pow(2, PWM_RES) - 1; // Max num: 511
const uint16_t MINIMUM_DUTY = 0;
const uint16_t MAX_TEMPERATURE = MAX_DUTY; // 511

struct user_lamp_info_t
{
    int32_t current_duty_ch[2] = {0};
    int32_t target_duty_ch[2] = {0};
    float led_temperature = 0.5f;
} user_lamp_info;

struct_message send_data{
    .msgType = DATA,
    .id = BOARD_ID,
    .command = SET_DUTY,
    .data = {0, 0}};

void readGetMacAddress()
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
    clientMacAddress[0] = baseMac[0];
    clientMacAddress[1] = baseMac[1];
    clientMacAddress[2] = baseMac[2];
    clientMacAddress[3] = baseMac[3];
    clientMacAddress[4] = baseMac[4];
    clientMacAddress[5] = baseMac[5];
}

void addPeer(const uint8_t *mac_addr, uint8_t chan)
{
    ESP_ERROR_CHECK(esp_wifi_set_channel(chan, WIFI_SECOND_CHAN_NONE));
    esp_now_del_peer(mac_addr);
    memset(&peer_info, 0, sizeof(esp_now_peer_info_t));
    peer_info.channel = chan;
    peer_info.encrypt = false;
    memcpy(peer_info.peer_addr, mac_addr, sizeof(uint8_t[6]));
    if (esp_now_add_peer(&peer_info) != ESP_OK)
    {
        Serial.println("Failed to add peer");
        return;
    }
    memcpy(serverAddress, mac_addr, sizeof(uint8_t[6]));
}

void printMAC(const uint8_t *mac_addr)
{
    char macStr[18];
    snprintf(macStr, sizeof(macStr), "%02x:%02x:%02x:%02x:%02x:%02x",
             mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
    Serial.print(macStr);
}

void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status)
{
    Serial.print("\r\nLast Packet Send Status:\t");
    Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Delivery Success" : "Delivery Fail");
    // if (status != ESP_NOW_SEND_SUCCESS && pairingStatus == PAIR_PAIRED)
    // {
    //     pairingStatus = PAIR_REQUEST;
    // }
}

void OnDataRecv(const uint8_t *mac_addr, const uint8_t *incomingData, int len)
{
    Serial.print("Packet received with ");
    Serial.print("data size = ");
    Serial.println(sizeof(incomingData));
    uint8_t type = incomingData[0];
    switch (type)
    {
    case DATA: // we received data from server
        memcpy(&inData, incomingData, sizeof(inData));
        if (inData.command == SET_DUTY)
        {
            user_lamp_info.current_duty_ch[0] = inData.data[0];
            user_lamp_info.current_duty_ch[1] = inData.data[1];
            if (inData.data[0] + inData.data[1] == 0)
            {
                ledState = LED_POWER_OFF;
            }
            else
            {
                ledState = LED_POWER_ON;
            }
        }
        else if (inData.command == POWER_OFF)
        {
            ledState = LED_POWER_OFF;
        }
        break;

    case PAIRING: // we received pairing data from server
        memcpy(&pairingData, incomingData, sizeof(pairingData));
        if (pairingData.id == 0)
        { // the message comes from server
            Serial.print("Pairing done for MAC Address: ");
            printMAC(pairingData.macAddr);
            Serial.print(" on channel ");
            Serial.print(pairingData.channel); // channel used by the server
            Serial.print(" in ");
            Serial.print(millis() - start_time);
            Serial.println("ms");
            addPeer(pairingData.macAddr, pairingData.channel); // add the server  to the peer list

            pairingStatus = PAIR_PAIRED; // set the pairing status
            saveMyPreference("my_preference", "peer_info", &peer_info, sizeof(esp_now_peer_info_t));
        }
        break;
    }
}

PairingStatus autoPairing()
{
    switch (pairingStatus)
    {
    case HAVE_RECORD:
        Serial.print("ESP NOW have paring record: ");
        printMAC(peer_info.peer_addr);
        Serial.printf("  channel is: %d \n", peer_info.channel);

        // set WiFi channel
        ESP_ERROR_CHECK(esp_wifi_set_channel(peer_info.channel, WIFI_SECOND_CHAN_NONE));
        if (esp_now_init() != ESP_OK)
        {
            Serial.println("Error initializing ESP-NOW");
        }

        // set callback routines
        esp_now_register_send_cb(OnDataSent);
        esp_now_register_recv_cb(esp_now_recv_cb_t(OnDataRecv));

        if (esp_now_add_peer(&peer_info) != ESP_OK)
        {
            Serial.println("Failed to add peer");
            pairingStatus = PAIR_REQUEST;
            break;
        }
        Serial.println("ESP NOW init sucessfully from NVS!");
        pairingStatus = PAIR_PAIRED;
        break;

    case PAIR_REQUEST:
        Serial.print("Pairing request on channel ");
        Serial.println(channel);
        ESP_ERROR_CHECK(esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_NONE));
        if (esp_now_init() != ESP_OK)
        {
            Serial.println("Error initializing ESP-NOW");
        }

        // set callback routines
        esp_now_register_send_cb(OnDataSent);
        esp_now_register_recv_cb(esp_now_recv_cb_t(OnDataRecv));

        // set pairing data to send to the server
        pairingData.msgType = PAIRING;
        pairingData.id = BOARD_ID;
        pairingData.channel = channel;
        memcpy(pairingData.macAddr, clientMacAddress, sizeof(clientMacAddress));

        // add peer and send request
        addPeer(full_mac_address, channel);
        esp_now_send(full_mac_address, (uint8_t *)&pairingData, sizeof(pairingData));
        previousMillis = millis();
        pairingStatus = PAIR_REQUESTED;
        break;

    case PAIR_REQUESTED:
        // time out to allow receiving response from server
        currentMillis = millis();
        if (currentMillis - previousMillis > 1000)
        {
            previousMillis = currentMillis;
            // time out expired,  try next channel
            channel++;
            if (channel > MAX_CHANNEL)
            {
                channel = 1;
                pairing_retry_counter++;
            }
            if (pairing_retry_counter > MAX_RETRY_COUNT)
            {
                Serial.println("esp now pairing time out, enter deep sleep!");
                pairing_retry_counter = 0;
                esp_deep_sleep_start();
            }
            pairingStatus = PAIR_REQUEST;
        }
        break;

    case PAIR_PAIRED:
        // nothing to do here
        break;
    case NOT_PAIRED:
        // nothing to do here
        break;
    }
    return pairingStatus;
}

/**********LED Controller***********/

void update_target_duty(int32_t change_value)
{
    static int32_t led_luminance = 0;
    int32_t change_luminance = change_value * LUMINANCE_CHANGE_WEIGHT;
    led_luminance += change_luminance;

    if (led_luminance < 0)
    {
        led_luminance = 0;
    }
    if (led_luminance > MAX_DUTY)
    {
        led_luminance = MAX_DUTY;
    }

    user_lamp_info.target_duty_ch[0] = led_luminance * user_lamp_info.led_temperature;
    user_lamp_info.target_duty_ch[1] = led_luminance * (1.0f - user_lamp_info.led_temperature);
}

void update_led_temperature(int32_t change_value)
{
    int32_t change_temperature = change_value * TEMPERTURE_CHANGE_WEIGHT;
    static int32_t led_temperature = MAX_TEMPERATURE / 2;

    led_temperature += change_temperature;
    if (led_temperature <= 0)
    {
        led_temperature = 0;
    }
    if (led_temperature >= MAX_TEMPERATURE)
    {
        led_temperature = MAX_TEMPERATURE;
    }

    user_lamp_info.led_temperature = float(led_temperature) / float(MAX_TEMPERATURE); // normalize to [0, 1]
}

AiEsp32RotaryEncoder rotaryEncoder = AiEsp32RotaryEncoder(
    ROTARY_ENCODER_A_PIN,
    ROTARY_ENCODER_B_PIN,
    -1,
    ROTARY_ENCODER_STEPS);

int32_t last_encoder_value = 0;

void ARDUINO_ISR_ATTR readEncoderISR()
{
    rotaryEncoder.readEncoder_ISR();
}

/*custom function*/
bool areMacEqual(const uint8_t mac1[], const uint8_t mac2[], int size)
{
    if (mac1 == nullptr || mac2 == nullptr)
    {
        return false; // 或者根据具体需求选择其他处理方式
    }
    for (int i = 0; i < size; ++i)
    {
        if (mac1[i] != mac2[i])
        {
            return false; // 发现不相等元素，立即返回false
        }
    }
    return true; // 循环结束，所有元素都相等
}

void saveMyPreference(const char *name, const char *key, const void *value, size_t len)
{
    preference.begin(name, false);
    preference.putBytes(key, value, len);
    preference.end();
}

void esp_now_send_data(struct_message my_send_data)
{
    // if (pairingStatus != PAIR_PAIRED)
    // {
    //     Serial.println("ESP NOW not paired, cannot send data!");
    //     return;
    // }
    send_data.command = my_send_data.command;
    send_data.data[0] = my_send_data.data[0];
    send_data.data[1] = my_send_data.data[1];
    esp_err_t result = esp_now_send(serverAddress, (uint8_t *)&send_data, sizeof(send_data));
    if (result != ESP_OK)
    {
        Serial.printf("ESP NOW Send data error, ERR code: 0X%x", result);
    }
}

void led_power_on()
{
    esp_now_send_data(
        {.msgType = DATA,
         .id = BOARD_ID,
         .command = POWER_ON,
         .data = {user_lamp_info.target_duty_ch[0], user_lamp_info.target_duty_ch[1]}});
}
void led_power_off()
{
    esp_now_send_data(
        {.msgType = DATA,
         .id = BOARD_ID,
         .command = POWER_OFF,
         .data = {0, 0}});
}

void on_button_short_click(Button2 &btn)
{
    led_power_on();
    Serial.print("button SHORT press ");
    //   Serial.print(millis());
    //   Serial.println(" milliseconds after restart");
}

void on_button_long_click(Button2 &btn)
{
    led_power_off();
    Serial.print("button LONG press ");
    //   Serial.print(millis());
    //   Serial.println(" milliseconds after restart");
}

void on_button_double_click(Button2 &btn)
{
    Serial.print("button DOUBLE press ");
    //   Serial.print(millis());
    //   Serial.println(" milliseconds after restart");
}

void setup()
{
    Serial.begin(115200);

    // bool format_test = true;
    // if (format_test)
    // {
    //     preference.clear();
    // }

    // // Get peer info from NVS
    // uint8_t zero_mac_addr[6] = {0};
    // preference.begin("my_preference", false);
    // preference.getBytes("peer_info", &peer_info, sizeof(esp_now_peer_info_t));
    // preference.getBytes("user_lamp_info", &user_lamp_info, sizeof(user_lamp_info_t));
    // Serial.print("Get server mac address from NVS is: ");
    // printMAC(peer_info.peer_addr);
    // preference.end();
    // if (areMacEqual(peer_info.peer_addr, zero_mac_addr, sizeof(zero_mac_addr)))
    // {
    //     preference.putBytes("server_mac_addr", serverAddress, sizeof(serverAddress));
    //     pairingStatus = PAIR_REQUEST;
    // }
    // else
    // {
    //     memcpy(serverAddress, peer_info.peer_addr, sizeof(peer_info.peer_addr));
    //     pairingStatus = HAVE_RECORD;
    // }

    // attempt to connect to Wifi network:
    WiFi.mode(WIFI_STA);
    // esp_wifi_set_ps(WIFI_PS_MIN_MODEM);

    // esp_wifi_set_max_tx_power(20);       //1 unit is 0.25dBm, 20 is 5dBm | 3.16mW, default 80| 20dBm
    WiFi.STA.begin();
    // WiFi.disconnect(true); // disconnect from any previous connection

    Serial.print("Client Board MAC Address:  ");
    readGetMacAddress();
    // WiFi.disconnect();

    // esp now initialize
    if (esp_now_init() != ESP_OK)
    {
        Serial.println("Error initializing ESP-NOW");
        return;
    }
    // set callback routines
    esp_now_register_send_cb(OnDataSent);
    esp_now_register_recv_cb(esp_now_recv_cb_t(OnDataRecv));
    Serial.println("ESP NOW init sucessfully!");
    Serial.print("ESP NOW channel is: ");
    Serial.println(channel);

    memcpy(peer_info.peer_addr, serverAddress, sizeof(serverAddress));
    peer_info.channel = 1;
    peer_info.encrypt = false; // not encrypt data
    // addPeer(serverAddress, 4); // add the server to the peer list

    if (esp_now_add_peer(&peer_info) != ESP_OK)
    {
        Serial.println("Failed to add peer");
        pairingStatus = PAIR_REQUEST;
    }
    else
    {
        Serial.println("Peer added successfully");
        pairingStatus = PAIR_PAIRED;
    }

    // // Rotary encoder initialize
    rotaryEncoder.areEncoderPinsPulldownforEsp32 = false;
    rotaryEncoder.begin();
    rotaryEncoder.setup(readEncoderISR);
    rotaryEncoder.setBoundaries(-99999, 99999, false);
    rotaryEncoder.setAcceleration(50);
    last_encoder_value = rotaryEncoder.readEncoder();

    // Encoder button initialize
    encoderButton.begin(ENCODER_BUTTON_PIN, INPUT_PULLUP, true);
    encoderButton.setDebounceTime(50);
    encoderButton.setLongClickTime(2000);
    encoderButton.setDoubleClickTime(300);
    encoderButton.setLongClickDetectedHandler(on_button_long_click);
    encoderButton.setClickHandler(on_button_short_click);
    encoderButton.setDoubleClickHandler(on_button_double_click);

    // // ESP NOW initalize

    start_time = millis();

    // GPIO initalize
    pinMode(LED2_PIN, OUTPUT);

    // set up deep sleep
    uint64_t bitmask = BUTTON_PIN_BITMASK(ROTARY_ENCODER_A_PIN) | BUTTON_PIN_BITMASK(ROTARY_ENCODER_B_PIN);
    esp_deep_sleep_enable_gpio_wakeup(bitmask, ESP_GPIO_WAKEUP_GPIO_LOW);
    // pinMode(ROTARY_ENCODER_A_PIN, INPUT_PULLDOWN);
    // pinMode(ROTARY_ENCODER_B_PIN, INPUT_PULLDOWN);
    gpio_pullup_dis(GPIO_NUM_2);
    gpio_pulldown_en(GPIO_NUM_2);
    gpio_pullup_dis(GPIO_NUM_3);
    gpio_pulldown_en(GPIO_NUM_3);
    // gpio_pullup_dis(GPIO_NUM_4);
    // gpio_pulldown_en(GPIO_NUM_4);

    //
}

static uint32_t one_second_timer = millis();
static uint32_t deep_sleep_check_time = millis();

void loop()
{
    // paring
    // while (autoPairing() != PAIR_PAIRED);

    // if(millis() - one_second_timer >= 1000)
    // {
    //     digitalWrite(LED2_PIN, !digitalRead(LED2_PIN));
    //     one_second_timer = millis();

    // if ((user_lamp_info.current_duty_ch[0] != user_lamp_info.target_duty_ch[0]) ||
    //     (user_lamp_info.current_duty_ch[1] != user_lamp_info.target_duty_ch[1])) {
    //         pairingStatus = PAIR_REQUEST;
    // }
    // }

    // if (millis() - deep_sleep_check_time >= 8000 && pairingStatus == PAIR_PAIRED)
    // {
    //     saveMyPreference("my_preference", "user_lamp_info", &user_lamp_info, sizeof(user_lamp_info_t));
    //     saveMyPreference("my_preference", "peer_info", &peer_info, sizeof(esp_now_peer_info_t));
    //     Serial.println("System into deep light mode, Good bye~");
    //     esp_deep_sleep_start();
    // }

    encoderButton.loop();

    if (rotaryEncoder.encoderChanged())
    {
        deep_sleep_check_time = millis();

        // Serial.println("encoderChanged");
        int32_t encoder_change_value = rotaryEncoder.readEncoder() - last_encoder_value;
        Serial.printf("Encoder change value: %ld\n", encoder_change_value);
        rotary_direction = encoder_change_value > 0 ? 1 : -1; // CW: 1, ACW: -1
        last_encoder_value = rotaryEncoder.readEncoder();
        //        rotaryEncoder.setEncoderValue(0); // reset encoder value to 0 after reading

        if (encoderButton.isPressed())
        {
            Serial.println("Encoder button pressed");
            update_led_temperature(encoder_change_value);
            update_target_duty(0);
        }
        else
        {
            update_target_duty(encoder_change_value);
        }

        Serial.printf("Current duty ch1: %ld, ch2: %ld\n",
                      user_lamp_info.current_duty_ch[0],
                      user_lamp_info.current_duty_ch[1]);
        Serial.printf("Target duty ch1: %ld, ch2: %ld, temperature: %.2f\n",
                      user_lamp_info.target_duty_ch[0],
                      user_lamp_info.target_duty_ch[1],
                      user_lamp_info.led_temperature);

        if ((user_lamp_info.target_duty_ch[0] != user_lamp_info.current_duty_ch[0]) || (user_lamp_info.target_duty_ch[1] != user_lamp_info.current_duty_ch[1]))
        {
            esp_now_send_data({.msgType = DATA,
                               .id = BOARD_ID,
                               .command = SET_DUTY,
                               .data = {user_lamp_info.target_duty_ch[0], user_lamp_info.target_duty_ch[1]}});

            Serial.printf("Send data: SET_DUTY, ch1: %ld, ch2: %ld\n",
                          user_lamp_info.target_duty_ch[0],
                          user_lamp_info.target_duty_ch[1]);
        }
    }

    // for (int i = 1; i < 13; i++)
    // {
    //     // peer_info.channel = i;
    //     addPeer(serverAddress, i); // add the server to the peer list
    //     esp_now_send_data({.msgType = DATA,
    //                        .id = BOARD_ID,
    //                        .command = SET_DUTY,
    //                        .data = {user_lamp_info.target_duty_ch[0], user_lamp_info.target_duty_ch[1]}});

    //     delay(1000); // wait for 1 second before next iteration
    // }
}
