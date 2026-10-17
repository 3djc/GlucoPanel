#pragma once
#include <Arduino.h>

// `len` random characters from `alphabet`, from the hardware random number generator (the system
// generator in the simulator). Used for the settings code, the web session and the hotspot password.
String randomToken(int len, const char* alphabet);
