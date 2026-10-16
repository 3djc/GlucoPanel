#pragma once
#include "TFT_eSPI.h"

#define AP_NAME "GlucoPanel"

// Connects with saved credentials; otherwise opens the "GlucoPanel" hotspot with a
// captive portal and shows QR codes on the display. Blocks until connected.
bool wifiConnect(EPaper& epaper);

// Draws `text` as a QR code (version 4, ~62 bytes max) with its top-left module at (x, y).
void drawQR(EPaper& epaper, int x, int y, int scale, const char* text);
