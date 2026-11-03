#pragma once

#define AP_NAME "GlucoPanel"

// Connects with saved credentials; otherwise opens the "GlucoPanel" hotspot with a
// captive portal and shows QR codes on the display. Blocks until connected.
bool wifiConnect();

#ifdef ARDUINO
// Light sleep (power.h): the radio is off while the panel sleeps. wifiResume() joins the saved network again.
void wifiSuspend();
bool wifiResume(unsigned long timeoutMs);
#endif

// Shows a message, erases the saved WiFi and restarts; the panel then comes back in setup mode.
[[noreturn]] void wifiEraseAndRestart();
