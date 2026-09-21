#include <Arduino.h>
#include <WiFi.h>

#include "config.h"
#include "core0.h"
#include "btAudio.h"
#include <esp_a2dp_api.h>

TaskHandle_t Task0;
btAudio btaudio = btAudio("Bender BT");



void setup() {
    xTaskCreatePinnedToCore(core0, "Task0", 10000, NULL, 1, &Task0, 0);
    Serial.begin(115200);
   
    change_state();
}

void loop() {
    if (reconnect) {
        btaudio.volume(15);
        reconnect = nullptr;
    }
}

