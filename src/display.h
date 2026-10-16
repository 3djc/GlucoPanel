#pragma once
#include "TFT_eSPI.h"
#include "glucose.h"

// Screen = 3 rows x 3 blocks. Row 1: trend arrow | delta vs previous reading | delta vs 15 min ago.
// Row 2: (empty) | glucose value | (empty). Row 3: empty. A thin strip at the very bottom shows the
// reading's age and the last fetch error. When nothing is configured it shows the settings page address.
void drawDashboard(EPaper& e, const Reading& last, const Deltas& deltas, const String& error,
                   bool notConfigured, const String& configUrl);

// Call from loop(): once a minute, refreshes only the age text (partial refresh).
void dashboardTick(EPaper& e);
