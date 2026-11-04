#pragma once

// Battery level of the reTerminal E1001 (the LiPo behind the e-paper). The voltage is read on GPIO1 through a
// divider by 2 that GPIO21 switches on, once at start and then every hour (PERIOD_HOURS in battery.cpp; the level moves
// slowly). Boards without this circuit (the LCD profile) report "unknown". The simulator reads the voltage in mV from the SIM_BATTERY_MV environment variable, to preview the icon.

// Call from loop(): reads the battery the first time and then every hour.
void batteryTick();

// 0..100, or -1 when the board has no battery monitor or no reading looks valid.
int batteryPercent();

// The last battery voltage in volts (0 when unknown).
float batteryVolts();

// True while the USB cable is plugged in, so the battery level means little. On the E1001 the USB-serial bridge
// is powered by the USB: its TX output, wired to GPIO44, idles high when plugged and is not driven otherwise (a
// pull-down on GPIO44 makes that a clean 0). The simulator says "plugged" when SIM_USB is set.
bool batteryUsbPowered();
