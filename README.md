# GlucoPanel

Glucose display firmware for the Seeed **reTerminal E1001** (ESP32-S3, 7.5" 800×480 monochrome ePaper, no touch), built with PlatformIO and the Arduino framework. It includes an SDL2 simulator so you can develop without the device.

> **Status:** WiFi onboarding, the settings web page and the simulator work. Fetching and drawing glucose data is not implemented yet; the settings are stored but nothing reads them.

## How it works

1. **Boot.** The panel tries the saved WiFi (up to 20 s).
2. **Saved WiFi can't be joined.** The screen asks whether to erase it:
   - **KEY0 (green):** erase the credentials and go to setup.
   - **KEY1:** retry. With no key pressed it retries by itself after 60 s.
3. **No WiFi saved (or erased).** The panel opens an open hotspot named **`GlucoPanel`** with a captive portal. The screen shows two QR codes: one joins the hotspot, one opens `http://192.168.4.1`.
4. **Connected.** The screen shows the SSID, the IP and a QR code for the settings page, which is also at `http://glucopanel.local`.

### Settings page

Open the address shown on the display from any device on the same network.

| Setting | Notes |
| --- | --- |
| Data source | Nightscout (URL, optional token), LibreLinkUp (email, password, region) or Dexcom Share (username, password, server) |
| Display unit | mg/dL or mmol/L |
| Low / high thresholds | Always entered in mg/dL; low must be below high |
| Erase WiFi | Wipes the saved network and reboots into the `GlucoPanel` hotspot |

The refresh interval is not a setting: it is fixed by the source (5 min for Dexcom, 1 min for LibreLinkUp and Nightscout). See `refreshMinutes()`.

Saved values live in the ESP32's flash (NVS); read them with `cfgGet("key")`. Password fields are never echoed back, and leaving one blank keeps the stored value.

**Security:** the page has no login and secrets are stored unencrypted. Only use it on a network you trust.

## Build and flash

Requires [PlatformIO](https://platformio.org/).

```sh
pio run -e reterminal_e1001               # build
pio run -e reterminal_e1001 -t upload     # flash
pio device monitor                        # serial, 115200 baud
```

`src/driver.h` selects the board and screen for the Seeed_GFX library (`BOARD_SCREEN_COMBO 520` = reTerminal E1001). For other boards see the [Seeed ePaper + PlatformIO guide](https://wiki.seeedstudio.com/epaper_work_with_platformio/).

## Simulator

The `sim` environment builds the same `src/` code for your computer, with an SDL2 window standing in for the ePaper and a small HTTP server standing in for the device's web server.

```sh
brew install sdl2            # macOS, once
pio run -e sim -t exec
```

- The window is 800×480. **Esc** or closing it quits.
- **Enter** on the setup screen pretends the WiFi setup finished.
- The settings page is at <http://localhost:8080>.
- Keys **1** / **2** stand in for KEY0 / KEY1 (GPIO 3 / 4).
- `SIM_WIFI=fail` simulates saved WiFi that can't be joined.
- Settings are saved in `.sim_prefs` (git-ignored).

Other variables:

| Variable | Effect |
| --- | --- |
| `SIM_NOWAIT=1` | Skip waiting for Enter on the setup screen |
| `SIM_PRESS=<pin>` | Hold that button pin pressed (3 or 4) |
| `SIM_FRAMES=<n>` | Exit after `n` calls to `loop()` |
| `SIM_SCREENSHOT=out_%d.bmp` | Save a BMP at every display update |

Limitations: text uses a scaled 5×7 font, so it looks different from the real fonts; ePaper refresh time and ghosting are not modelled; WiFi, HTTP clients and touch are not simulated.

## Layout

```
platformio.ini      environments: reterminal_e1001 (device), sim (SDL2)
src/
  main.cpp          setup/loop
  driver.h          Seeed_GFX board selection
  wifi_setup.*      saved-WiFi join, erase prompt, hotspot + QR codes
  config.*          settings table and NVS storage
  config_server.*   settings web page
sim/
  sim_main.cpp      SDL2 host and drawing
  include/          Arduino, WiFi, WebServer, Preferences... stand-ins
```

## Notes

- Include order matters in `wifi_setup.cpp` and `config_server.cpp`: Seeed_GFX defines `FS_NO_GLOBALS`, which breaks `WebServer.h` if its headers come first.
- Button pins (GPIO 3 and 4, active low) come from the [Seeed Arduino peripherals cookbook](https://wiki.seeedstudio.com/reterminal_e10xx_with_arduino_peripherals/); confirm them on your board.
