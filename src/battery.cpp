#include "battery.h"
#include <Arduino.h>

#if defined(ARDUINO) && !defined(UI_PROFILE_LCD)
#define BATTERY_MONITOR 1
#else
#define BATTERY_MONITOR 0
#include <stdlib.h>
#endif

namespace {

const unsigned long PERIOD_HOURS = 1;  // 1 hour while the E1001 is being tested; 12 is enough in use
const unsigned long PERIOD_MS = PERIOD_HOURS * 3600UL * 1000UL;
const int BATTERY_ADC_PIN = 1;      // GPIO1: the battery voltage through a divider by 2
const int BATTERY_ENABLE_PIN = 21;  // GPIO21 high switches the divider on (it leaks when left on)
const int USB_BRIDGE_PIN = 44;      // UART0 RX: the USB-serial bridge's TX

bool haveReading = false;
unsigned long lastRead = 0;
float volts = 0;
int percent = -1;

// Resting voltage of a single LiPo cell -> state of charge. A rough curve: the voltage also depends on the load
// and on the charger, but it is plenty for an icon.
int percentFromVolts(float v) {
  static const struct { float v; int pct; } curve[] = {
      {4.20f, 100}, {4.15f, 95}, {4.11f, 90}, {4.08f, 85}, {4.02f, 80}, {3.98f, 75}, {3.95f, 70},
      {3.91f, 65},  {3.87f, 60}, {3.85f, 55}, {3.84f, 50}, {3.82f, 45}, {3.80f, 40}, {3.79f, 35},
      {3.77f, 30},  {3.75f, 25}, {3.73f, 20}, {3.71f, 15}, {3.69f, 10}, {3.61f, 5},  {3.27f, 0},
  };
  const int n = sizeof(curve) / sizeof(curve[0]);
  if (v >= curve[0].v) return 100;
  if (v <= curve[n - 1].v) return 0;
  for (int i = 1; i < n; i++) {
    if (v >= curve[i].v) {
      float t = (v - curve[i].v) / (curve[i - 1].v - curve[i].v);
      return (int)(curve[i].pct + t * (curve[i - 1].pct - curve[i].pct) + 0.5f);
    }
  }
  return 0;
}

#if BATTERY_MONITOR
float readVolts() {
  pinMode(BATTERY_ENABLE_PIN, OUTPUT);
  digitalWrite(BATTERY_ENABLE_PIN, HIGH);
  analogReadResolution(12);  // the default attenuation (11 dB, up to about 3.1 V) fits half of 4.2 V
  delay(10);  // the divider settles
  uint32_t sum = 0;
  for (int i = 0; i < 16; i++) {
    sum += analogReadMilliVolts(BATTERY_ADC_PIN);
    delay(2);
  }
  digitalWrite(BATTERY_ENABLE_PIN, LOW);
  return sum / 16.0f * 2.0f / 1000.0f;
}
#else
float readVolts() {
  const char* mv = getenv("SIM_BATTERY_MV");
  return mv ? (float)atof(mv) / 1000.0f : 0;
}
#endif

}  // namespace

void batteryTick() {
  if (haveReading && millis() - lastRead < PERIOD_MS) return;
  haveReading = true;
  lastRead = millis();
  volts = readVolts();
  percent = volts > 3.0f ? percentFromVolts(volts) : -1;  // nothing sensible: no battery, or no monitor
  if (percent >= 0) Serial.printf("battery: %.3f V (%d%%)\n", volts, percent);
}

int batteryPercent() { return percent; }

float batteryVolts() { return volts; }

bool batteryUsbPowered() {
#if BATTERY_MONITOR
  pinMode(USB_BRIDGE_PIN, INPUT_PULLDOWN);  // left like this, it also keeps the line from floating while asleep
  delayMicroseconds(200);                   // the line settles
  return digitalRead(USB_BRIDGE_PIN) == HIGH;
#else
  return getenv("SIM_USB") != nullptr;
#endif
}
