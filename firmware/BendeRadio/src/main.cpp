#include <Arduino.h>

#include "btAudio.h"
#include "config.h"
#include "core0.h"

TaskHandle_t Task0;
btAudio btaudio = btAudio("Bender BT");

void setup() {
    Serial.begin(115200);
    delay(500);
    Serial.println(F("\n=== BOOT ==="));
    xTaskCreatePinnedToCore(core0, "Task0", 20480, NULL, 1, &Task0, 0);
}

void loop() {
    vTaskDelay(pdMS_TO_TICKS(100));
}
