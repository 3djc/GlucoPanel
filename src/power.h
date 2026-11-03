#pragma once

// Light sleep between glucose fetches. The Arduino-ESP32 libraries are built without ESP-IDF power management
// (no automatic light sleep), so the sleep is manual: the WiFi radio is switched off for the wait, the chip
// light-sleeps until the next clock minute or fetch, and a button press wakes it. Only on the e-paper panel:
// the LCD stays on and is touched, and the simulator has no sleep. Build with -DPOWER_SAVE=0 to turn it off.
#if !defined(POWER_SAVE)
#if defined(ARDUINO) && !defined(UI_PROFILE_LCD)
#define POWER_SAVE 1
#else
#define POWER_SAVE 0
#endif
#endif
