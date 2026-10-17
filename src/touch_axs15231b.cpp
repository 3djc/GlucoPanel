// Capacitive touch of the Guition JC3248W535: the AXS15231B reports touches over I2C (device only).
// Protocol and mapping confirmed with tools/bringup_jc3248w535.
#if defined(ARDUINO) && defined(UI_PROFILE_LCD)
#include "touch.h"
#include <Arduino.h>
#include <Wire.h>

static const uint8_t ADDR = 0x3B;
static const int PIN_SDA = 4, PIN_SCL = 8, PIN_INT = 11, PIN_RST = 12;
// The controller reports a point once per scan, not on every read: between two scans a read returns "no
// touch". A touch only counts as released after this long without a point.
static const unsigned long RELEASE_AFTER_MS = 150;

void touchBegin() {
  Wire.begin(PIN_SDA, PIN_SCL);
  Wire.setClock(400000);
  pinMode(PIN_INT, INPUT_PULLUP);
  pinMode(PIN_RST, OUTPUT);
  digitalWrite(PIN_RST, LOW);
  delay(200);
  digitalWrite(PIN_RST, HIGH);
  delay(200);
}

// Reads one report: [gesture, points, x hi/lo, y hi/lo, ...]. True when a valid point was returned.
static bool readPoint(int& x, int& y) {
  const uint8_t cmd[11] = {0xb5, 0xab, 0xa5, 0x5a, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00};
  Wire.beginTransmission(ADDR);
  Wire.write(cmd, sizeof(cmd));
  if (Wire.endTransmission() != 0) return false;
  uint8_t d[8];
  if (Wire.requestFrom((int)ADDR, 8) != 8) return false;
  for (int i = 0; i < 8; i++) d[i] = Wire.read();
  if (d[1] != 1) return false;  // one touch point
  int rawX = ((d[2] & 0x0F) << 8) | d[3], rawY = ((d[4] & 0x0F) << 8) | d[5];
  if (rawX > 500 || rawY > 500) return false;  // garbage at the edges
  y = 320 - constrain(rawX, 0, 320);           // the panel is portrait; this is the landscape orientation
  x = constrain(rawY, 0, 479);
  return true;
}

TouchPoint touchPoll() {
  static TouchPoint last;
  static unsigned long lastSeen = 0;
  int x, y;
  unsigned long now = millis();
  if (readPoint(x, y)) {
    last = {true, x, y};
    lastSeen = now;
  } else if (last.down && now - lastSeen >= RELEASE_AFTER_MS) {
    last.down = false;
  }
  return last;
}
#endif
