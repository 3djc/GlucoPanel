#pragma once

// Touch input (UI_PROFILE_LCD only; the e-paper profile has buttons). Coordinates are screen pixels in the
// landscape orientation of the profile. Device: touch_axs15231b.cpp. Simulator: the mouse (sim/sim_main.cpp).
#ifdef UI_PROFILE_LCD
struct TouchPoint {
  bool down = false;
  int x = 0, y = 0;
};
void touchBegin();
TouchPoint touchPoll();  // call every loop; a touch counts as released after a short gap without a point
#endif
