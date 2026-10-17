#include "input.h"
#include <Arduino.h>
#include "touch.h"

#ifdef UI_PROFILE_LCD
// ---------------------------------------------------------------------------------------------------
// Touch screen: one finger, one gesture at a time
// ---------------------------------------------------------------------------------------------------
static bool isDown = false, holdFired = false;
static unsigned long since = 0;
static int startX = 0, startY = 0, lastX = 0, lastY = 0;

void inputBegin() { touchBegin(); }

Input inputPoll() {
  Input in;
  TouchPoint t = touchPoll();
  unsigned long now = millis();
  if (t.down) {
    if (!isDown) {
      isDown = true;
      holdFired = false;
      since = now;
      startX = t.x;
      startY = t.y;
    }
    lastX = t.x;
    lastY = t.y;
    in.down = true;
    in.heldMs = now - since;
    in.x = startX;
    in.y = startY;
    if (!holdFired && in.heldMs >= LONG_PRESS_MS) {
      holdFired = true;
      in.gesture = G_HOLD;
    }
  } else if (isDown) {
    isDown = false;
    in.x = lastX;
    in.y = lastY;
    if (!holdFired && now - since >= 30) in.gesture = G_TAP;  // shorter touches are noise
  }
  return in;
}

#else
// ---------------------------------------------------------------------------------------------------
// Buttons (reTerminal E1001: active low, external pull-ups; GPIO numbers from the Seeed cookbook)
// ---------------------------------------------------------------------------------------------------
static const int BTN_GREEN = 3, BTN_OTHER = 4;

struct Button {
  int pin;
  bool down = false, holdFired = false;
  unsigned long since = 0;
};
static Button green{BTN_GREEN}, other{BTN_OTHER};

// Poll every loop; fills `in` when the button produced an event.
static bool pollButton(Button& b, Input& in, bool secondary) {
  bool low = digitalRead(b.pin) == LOW;
  unsigned long now = millis();
  if (low && !b.down) {
    b.down = true;
    b.holdFired = false;
    b.since = now;
  } else if (low && !b.holdFired && now - b.since >= LONG_PRESS_MS) {
    b.holdFired = true;
    in.gesture = G_HOLD;
    in.secondary = secondary;
    return true;
  } else if (!low && b.down) {
    b.down = false;
    if (!b.holdFired && now - b.since >= 40) {  // ignore contact bounce
      in.gesture = G_TAP;
      in.secondary = secondary;
      return true;
    }
  }
  return false;
}

void inputBegin() {
  pinMode(BTN_GREEN, INPUT);
  pinMode(BTN_OTHER, INPUT);
}

Input inputPoll() {
  Input in;
  if (!pollButton(green, in, false)) pollButton(other, in, true);
  in.down = green.down;
  in.heldMs = green.down ? millis() - green.since : 0;
  return in;
}
#endif
