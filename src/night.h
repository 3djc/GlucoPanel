#pragma once

// Night mode. Settings: "night_mode" (off / on / scheduled), "night_from" / "night_to" (local time, may run over
// midnight). At night the dashboard is shown white on black. On the colour LCD the backlight also goes to
// "night_level" (and to "brightness" the rest of the time); the e-paper has no backlight.
void nightTick();  // call from loop(); looks at the settings every couple of seconds
