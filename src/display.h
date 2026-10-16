#pragma once
#include "TFT_eSPI.h"
#include "glucose.h"

// Draws the main screen: last good reading (if any), trend arrow, range status, age and the
// last fetch error. When nothing is configured it shows the settings page address instead.
void drawDashboard(EPaper& e, const Reading& last, const String& error, bool notConfigured,
                   const String& configUrl);

// Call from loop(): once a minute, refreshes only the "Updated N min ago" line (partial refresh).
void dashboardTick(EPaper& e);
