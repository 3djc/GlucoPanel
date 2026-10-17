#pragma once

// One input layer for both profiles. E-paper (reTerminal E1001): two buttons, KEY0 (green) and KEY1.
// Colour LCD (JC3248W535): the touch screen (touch.h).
//   G_TAP  a press that ended before LONG_PRESS_MS (reported on release)
//   G_HOLD the press has lasted LONG_PRESS_MS (reported once, while it is still down)
static const unsigned long LONG_PRESS_MS = 3000;

enum Gesture { G_NONE, G_TAP, G_HOLD };

struct Input {
  Gesture gesture = G_NONE;
  int x = -1, y = -1;      // LCD: where the touch started (HOLD, down) or ended (TAP); -1 on e-paper
  bool secondary = false;  // e-paper: the event comes from KEY1, not from the green button
  bool down = false;       // the green button / the finger is down right now
  unsigned long heldMs = 0;  // ... for this long
};

void inputBegin();  // button pins / touch controller
Input inputPoll();  // call every loop
