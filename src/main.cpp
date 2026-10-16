#include <Arduino.h>
#include "TFT_eSPI.h"
#include "wifi_setup.h"

EPaper epaper;

void setup() {
  Serial.begin(115200);
  epaper.begin();
  wifiConnect(epaper);
}

void loop() {}
