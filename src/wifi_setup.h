#pragma once

#define AP_NAME "GlucoPanel"

// Connects with saved credentials; otherwise opens the "GlucoPanel" hotspot with a
// captive portal and shows QR codes on the display. Blocks until connected.
bool wifiConnect();

// Shows a message, erases the saved WiFi and restarts; the panel then comes back in setup mode.
[[noreturn]] void wifiEraseAndRestart();
