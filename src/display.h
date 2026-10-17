#pragma once
#include <Arduino.h>
#include "glucose.h"

// Screen = 3 rows x 3 blocks. Row 1: trend arrow | delta vs previous reading | delta vs 15 min ago.
// Row 2: (empty) | glucose value | (empty). Row 3: empty. A thin strip at the very bottom shows the
// reading's age and the last fetch error. When nothing is configured it shows the settings page address.
// Built with LVGL; only widgets whose content changed are redrawn and refreshed on the panel.
void drawDashboard(const Reading& last, const Deltas& deltas, const String& error,
                   bool notConfigured, const String& configUrl, const String& configCode);

// The settings screen (QR code) replaces the dashboard until dashboardHideConfig(); fetches keep updating
// the dashboard's data meanwhile, and it is redrawn when the screen is closed.
void dashboardShowConfig(const String& url, const String& code);
void dashboardHideConfig();

// Call from loop(): once a minute, refreshes only the age text (partial refresh).
void dashboardTick();
