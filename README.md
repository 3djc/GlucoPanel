# GlucoPanel

Glucose display firmware for the Seeed **reTerminal E1001** (ESP32-S3, 7.5" 800×480 monochrome ePaper, no touch), built with PlatformIO and the Arduino framework. It includes an SDL2 simulator so you can develop without the device.

> **Status:** WiFi onboarding, the settings page, glucose fetching (Nightscout, LibreLinkUp, Dexcom Share), the dashboard and the simulator are implemented. The fetchers are tested against a mock server only; they have not been run against the real services or on the device.

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

The panel then shows the latest reading, trend arrow, LOW / IN RANGE / HIGH and its age, and refetches on a timer. The refresh interval is not a setting: it is fixed by the source (5 min for Dexcom, 1 min for LibreLinkUp and Nightscout). See `refreshMinutes()`.

Saved values live in the ESP32's flash (NVS); read them with `cfgGet("key")`. Password fields are never echoed back, and leaving one blank keeps the stored value.

**Security:** the page has no login and secrets are stored unencrypted. Only use it on a network you trust.

## Data sources

| Source | How it is read |
| --- | --- |
| Nightscout | `GET <url>/api/v1/entries/sgv.json?count=1`, optional `&token=` (an access token; the raw API secret is not supported) |
| LibreLinkUp | `POST /llu/auth/login` (follows region redirects), then `GET /llu/connections` with `Authorization` and the SHA-256 `account-id` header; uses the first connection |
| Dexcom Share | `AuthenticatePublisherAccount`, `LoginPublisherAccountById`, then `ReadPublisherLatestGlucoseValues`; uses a username (not a phone/email login) |

LibreLinkUp and Dexcom Share are unofficial APIs and can change without notice. Sessions are cached and renewed on expiry. The clock is set over NTP because reading ages need it.

**Security:** HTTPS is used but the device does **not verify server certificates** (no CA bundle is embedded), so a hostile network could intercept your credentials. Pinning or bundling CA certificates would fix this.

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

To exercise the data sources without real accounts, run the mock server and point the sim at it:

```sh
python3 tools/mock_cgm.py                                   # terminal 1
SIM_API_BASE=http://localhost:9000 pio run -e sim -t exec   # terminal 2
```

Mock credentials: Dexcom `user` / `pass`; LibreLinkUp `a@b.c` / `pass` (`redirect@test` tests the region redirect); Nightscout token `tok`, any URL. The sim client needs libcurl, which ships with macOS.

Other variables:

| Variable | Effect |
| --- | --- |
| `SIM_API_BASE=<url>` | Send every HTTP request to this base URL (see above) |
| `SIM_NOWAIT=1` | Skip waiting for Enter on the setup screen |
| `SIM_PRESS=<pin>` | Hold that button pin pressed (3 or 4) |
| `SIM_FRAMES=<n>` | Exit after `n` calls to `loop()` |
| `SIM_SCREENSHOT=out_%d.bmp` | Save a BMP at every display update |

Text uses the same font data and metrics as the device (Seeed_GFX fonts 1, 2, 4, 6, 7 and 8, copied into `sim/fonts/`); the FreeFonts are not supported. Limitations: ePaper refresh time and ghosting are not modelled; WiFi and touch are not simulated.

## Layout

```
platformio.ini      environments: reterminal_e1001 (device), sim (SDL2)
src/
  main.cpp          setup/loop
  driver.h          Seeed_GFX board selection
  wifi_setup.*      saved-WiFi join, erase prompt, hotspot + QR codes
  config.*          settings table and NVS storage
  config_server.*   settings web page
  glucose.*         Nightscout / LibreLinkUp / Dexcom fetchers
  display.*         dashboard screen
  http.*            HTTP(S) client (device)
sim/
  sim_main.cpp      SDL2 host and drawing
  http_sim.cpp      HTTP client (libcurl)
  fonts/            Seeed_GFX font data used by the sim text renderer
  include/          Arduino, WiFi, WebServer, Preferences... stand-ins
tools/
  mock_cgm.py       mock CGM services for the simulator
```

## Notes

- Include order matters in `wifi_setup.cpp` and `config_server.cpp`: Seeed_GFX defines `FS_NO_GLOBALS`, which breaks `WebServer.h` if its headers come first.
- Button pins (GPIO 3 and 4, active low) come from the [Seeed Arduino peripherals cookbook](https://wiki.seeedstudio.com/reterminal_e10xx_with_arduino_peripherals/); confirm them on your board.
