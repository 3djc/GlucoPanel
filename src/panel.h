#pragma once
#include <stdint.h>

// Colour LCD (UI_PROFILE_LCD only). The e-paper profile talks to the panel library directly (lvgl_port.cpp).
// Device: panel_axs15231b.cpp. Simulator: sim/sim_main.cpp (an SDL window).
#ifdef UI_PROFILE_LCD
void panelBegin();                                                   // init the display; the backlight stays off until the first panelPresent()
void panelDraw(int x, int y, int w, int h, const uint16_t* rgb565);  // into the frame buffer (not on the screen yet)
void panelPresent();                                                 // push the frame buffer to the screen
void panelBacklight(int percent);                                    // 0..100; takes effect once the first frame is shown
#endif
