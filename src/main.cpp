#include <Arduino.h>
#include "TFT_eSPI.h"
#include "wifi_setup.h"
#include "config_server.h"

EPaper epaper;

void setup() {
  Serial.begin(115200);
  epaper.begin();
  if (wifiConnect(epaper)) configServerBegin();
}

void loop() {
  configServerHandle();
  delay(2);
}
