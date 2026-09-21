#include <Arduino.h>
#include <WiFi.h>

#include "config.h"
#include "core0.h"
#include "btAudio.h"
#include <esp_a2dp_api.h>

TaskHandle_t Task0;
btAudio btaudio = btAudio("Bender BT");

volatile bool bt_connected = false;

void a2dp_cb(esp_a2d_cb_event_t event, esp_a2d_cb_param_t* param) {
    if (event == ESP_A2D_CONNECTION_STATE_EVT) {
        bt_connected = (param->conn_stat.state == ESP_A2D_CONNECTION_STATE_CONNECTED);
        Serial.printf("[BT] connected = %d\n", bt_connected);
    }
}

void setup() {
    xTaskCreatePinnedToCore(core0, "Task0", 10000, NULL, 1, &Task0, 0);
    Serial.begin(115200);
    btaudio.begin();
    btaudio.reconnect();
    esp_a2d_register_callback(a2dp_cb);   // после begin()
    while (bt_connected == false) {
        anim_search();
    }
 
    change_state();
}

void loop() {
    if (reconnect) {
        btaudio.setVolume(15);
        reconnect = nullptr;
    }
    if (!bt_connected) {
        anim_search();
    }
}

