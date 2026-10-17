# GlucoPanel

Glucose display firmware for the Seeed **reTerminal E1001** (ESP32-S3, 7.5" 800×480 monochrome ePaper, no touch), built with PlatformIO, the Arduino framework and [LVGL](https://lvgl.io) 9.5 for the UI. It includes an SDL2 simulator so you can develop without the device.

![GlucoPanel dashboard in the simulator, with mock data](docs/screenshot.png)

> **Status:** WiFi onboarding, the settings page, glucose fetching (Nightscout, LibreLinkUp, Dexcom Share), the dashboard and the simulator are implemented. The fetchers are tested against a mock server only; they have not been run against the real services or on the device.

## Medical disclaimer

**GlucoPanel is not a medical device.** It is a hobby project that shows glucose readings fetched from third-party services, and it has not been tested, certified or approved for any medical use.

- **Do not use it to make treatment decisions**, such as insulin dosing or deciding what to eat. Always use your official sensor, reader or app, and its alarms.
- The displayed values can be delayed, wrong, stale or missing: the data comes through unofficial, undocumented APIs that can change or stop working without notice, over a network that can fail, and the panel can crash, lose power or freeze on an old reading. The reading's age is shown for that reason, but check it.
- The statistics (average, time in range, variability) and the **estimated HbA1c** are computed from the readings the panel happened to collect, with gaps; the HbA1c is an estimate (GMI) that can differ from a laboratory result by 0.5 % or more.
- The panel is not a replacement for medical advice. Talk to your healthcare team about your treatment.
- This project is not affiliated with or endorsed by Dexcom, Abbott (FreeStyle Libre / LibreLinkUp), Nightscout or Seeed Studio. The names are used only to say which services it can read.
- It is provided "as is", with no warranty of any kind; see the licence below.

## How it works

1. **Boot.** The panel tries the saved WiFi (up to 20 s).
2. **Saved WiFi can't be joined.** The screen asks whether to erase it:
   - **KEY0 (green):** erase the credentials and go to setup.
   - **KEY1:** retry. With no key pressed it retries by itself after 60 s.
3. **No WiFi saved (or erased).** The panel opens a hotspot named **`GlucoPanel`** with a captive portal. The hotspot is protected (WPA2) by a random password made on first use and kept in flash; the screen shows it as text and inside the first QR code, which joins the hotspot when scanned. The second QR code opens `http://192.168.4.1`.
4. **Connected.** The screen shows the SSID, the IP and a QR code for the settings page, which is also at `http://glucopanel.local`.

### Buttons

On the dashboard the green button (KEY0) does two things:

- **Short press:** shows a screen with a QR code, the address of the settings page and a new 6-digit code (see below). It closes after 60 s or when you press any button.
- **Hold for 3 s:** erases the saved WiFi and restarts in setup mode (the hotspot described above). This also works while the QR screen is up.

The buttons are only read once the panel is running; the boot-time WiFi prompt above uses KEY0 and KEY1 as described.

### Settings page

Open the address shown on the display from any device on the same network.

| Setting | Notes |
| --- | --- |
| Data source | Nightscout (URL, optional token), LibreLinkUp (email, password, region) or Dexcom Share (username, password, server) |
| Display unit | mg/dL or mmol/L |
| Block contents | What each of the seven blocks shows (see below) |
| HbA1c unit | % (NGSP, default) or mmol/mol (IFCC) for the estimated HbA1c block |
| Check server certificates | Yes (default): HTTPS servers must present a certificate signed by a known authority. No: accept any certificate (insecure) |
| Language | English (default), Français, Deutsch, Italiano or Español: the panel screens, the fetch errors and this page |
| Time zone | Used for the HH:MM clock on the dashboard (default Paris / Berlin / Madrid) |
| Low / high thresholds | Always entered in mg/dL; low must be below high |
| Erase WiFi | Wipes the saved network and reboots into the `GlucoPanel` hotspot |

The panel then shows the dashboard and refetches on a timer. The screen is a grid of seven blocks (no grid lines are drawn), and **each block's content is chosen in the settings page**, on a miniature of the dashboard: tap a block, then tap one of the preview cards to choose what it shows (plain drop-downs are used if JavaScript is off):

| | | |
| --- | --- | --- |
| top row, left (in the grey band) | top row, middle | top row, right |
| middle row, left | middle row, centre | middle row, right |
| bottom row (full width) | | |

Any of the six small blocks can show: nothing, the **glucose value** (big, with its unit), the **trend arrow**, the **change vs the previous reading**, the **change vs 15 min ago**, the **date and time** (local, see the time zone setting), the **time of the reading**, the **age of the reading** (minutes), the **freshness bar** (one segment per minute of age, up to 5), or one of four statistics over the collected history: the **average glucose**, the **time in range** (between your low and high limits, with the time below and above), the **lowest and highest** reading, the **variability** (coefficient of variation, with the standard deviation), or the **estimated HbA1c** (see below). The statistics are time-weighted and their caption says how many hours they cover: Dexcom Share returns about the last 16 h in one request, the panel keeps up to 24 h and builds the rest up while it runs (so the figures cover less right after a restart, and LibreLinkUp only offers its graph history). The bottom row shows the **glucose graph** or nothing. The default layout is arrow / change / change, clock / value / freshness bar, graph. The same content can be used in several blocks. The layout is rebuilt when the settings are saved.

Deltas are in the chosen unit and show `--` until enough history is known; history comes from the same API calls (the last `GRAPH_HOURS` = 3 h from Dexcom and Nightscout, LibreLinkUp via its graph endpoint). The top row is a rounded light-grey band (like Gluco-Monitor's) with the three cells separated by thin lines and small captions above the numbers. The E1001's real 4-level gray mode has no partial refresh, so the grey is a sparse black/white dot pattern (`LIGHT_GREY_DOTS` in `src/lvgl_port.cpp`, 0 to 16 dots per 4×4 pixels). The graph shows the last 3 h as a line (dot on the newest reading), with the low..high thresholds as a light-grey band and the y range following the data (at least 60..200 mg/dL). Readings more than 20 min apart are not joined. A thin strip under it shows the last fetch error. The refresh interval is not a setting: it is fixed by the source (5 min for Dexcom, 1 min for LibreLinkUp and Nightscout). See `refreshMinutes()`.

**Estimated HbA1c.** The block shows the *glucose management indicator* (GMI = 3.31 + 0.02392 × mean glucose in mg/dL, in %; mmol/mol = (% − 2.15) × 10.929). It is an estimate from sensor readings and can differ from a lab HbA1c by 0.5 % or more. Following the usual CGM rule it needs **14 days with at least 70 % of the readings**; until then the block says "Not available yet" and shows how many days of readings it has (for example "0.7 / 14 days"). Because that is far more than the RAM keeps, every reading is also added to per-day totals (seconds covered and glucose × seconds, 30 days, a few hundred bytes of text) saved in flash under the `hba1c` key every 30 min, so they survive a restart; a restart loses at most the last 30 min of counting, and gaps in the readings simply count as missing time. The totals belong to the account: they are discarded when the data source or its credentials (including the region) change.

Saved values live in the ESP32's flash (NVS); read them with `cfgGet("key")`. Password fields are never echoed back, and leaving one blank keeps the stored value.

**Access to the settings page.** The page needs a 6-digit **code that only the panel shows**: under the address on the "WiFi connected" and "Not configured" screens, and on the screen opened by a short press of the green button (a new code is made each time). The QR codes on those screens contain the code, so scanning one opens the page directly; otherwise open the address and type the code. A correct code starts a session (a cookie, 30 minutes after the last request); five wrong codes lock the page for a minute. Changing settings or erasing the WiFi therefore takes someone who can see the panel. The code is also printed on the serial console.

What this does **not** do: the page is still plain HTTP, so someone who can read your WiFi traffic can see the code and the session cookie; and secrets (the WiFi password, the service credentials) are stored unencrypted in the ESP32's flash, readable by someone with physical access and a cable. Flash encryption would fix the latter, but it is irreversible and makes development harder, so it is not used.

## ePaper refresh policy

Full refreshes flash and wear the panel, so `src/lvgl_port.cpp` and `src/display.cpp` avoid them:

- **Full refresh** only when the screen layout changes (not configured / no data / reading, or the source or unit changes) and every 30 minutes to clear ghosting (`FULL_REFRESH_MS`).
- **Partial refresh** of one window covering just the blocks that changed (arrow, deltas, value), the "Updated N min ago" strip once a minute, and the error text.
- **No refresh** when a fetch changes nothing, e.g. repeated identical errors.

The sim logs every refresh (`epaper: FULL refresh` / `partial refresh`) so you can check this.

## Data sources

| Source | How it is read |
| --- | --- |
| Nightscout | `GET <url>/api/v1/entries/sgv.json?count=1`, optional `&token=` (an access token; the raw API secret is not supported) |
| LibreLinkUp | `POST /llu/auth/login` (follows region redirects), then `GET /llu/connections` with `Authorization` and the SHA-256 `account-id` header; uses the first connection |
| Dexcom Share | `AuthenticatePublisherAccount`, `LoginPublisherAccountById`, then `ReadPublisherLatestGlucoseValues`; uses a username (not a phone/email login) |

LibreLinkUp and Dexcom Share are unofficial APIs and can change without notice. Sessions are cached and renewed on expiry. The clock is set over NTP because reading ages need it.

**Server certificates.** HTTPS connections are checked against the root certificate list that is part of the ESP32 SDK (the Mozilla list, about 68 KB, already in the firmware), so a hostile network cannot pose as Dexcom, LibreLinkUp or your Nightscout and read your credentials. Certificates have validity dates, so this needs the clock to be right; the panel waits for NTP before the first fetch. I checked that Dexcom Share and all LibreLinkUp regions chain to roots that are in that list (GTS Root R4 / GlobalSign Root CA); I could not test the check on a real device. If your Nightscout runs on its own server with a **self-signed certificate**, set "Check server certificates" to No in the settings (this disables the check for every service, so prefer a normal certificate, or plain `http://` on a network you trust). A failed check shows as a fetch error such as "X509 - Certificate verification failed".

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
- Keys **1** / **2** stand in for KEY0 / KEY1 (GPIO 3 / 4). Hold **1** for 3 s to see the WiFi erase.
- `SIM_WIFI=fail` simulates saved WiFi that can't be joined.
- Settings are saved in `.sim_prefs` (git-ignored).

To exercise the data sources without real accounts, run the mock server and point the sim at it:

```sh
python3 tools/mock_cgm.py                                   # terminal 1
SIM_API_BASE=http://localhost:9000 pio run -e sim -t exec   # terminal 2
```

Set `MOCK_PERIOD=2400` on the mock for a slow, realistic glucose curve (the default is a fast demo wave; the screenshot above was taken with it, with `SIM_SCREENSHOT`, so it shows mock data only). Set `MOCK_FREEZE=1` on the mock to keep one measurement time so the reading's age keeps growing. Mock credentials: Dexcom `user` / `pass`; LibreLinkUp `a@b.c` / `pass` (`redirect@test` tests the region redirect); Nightscout token `tok`, any URL. The sim client needs libcurl, which ships with macOS.

Other variables:

| Variable | Effect |
| --- | --- |
| `SIM_API_BASE=<url>` | Send every HTTP request to this base URL (see above) |
| `SIM_NOWAIT=1` | Skip waiting for Enter on the setup screen |
| `SIM_PRESS=<pin>` | Hold that button pin pressed (3 or 4) |
| `SIM_TAP=<pin>:<start ms>:<duration ms>[,...]` | Press that button during each time window, e.g. `3:4000:300` is a short press at 4 s, `3:3000:4500` a long one |
| `SIM_FRAMES=<n>` | Exit after `n` calls to `loop()` |
| `SIM_SCREENSHOT=out_%d.bmp` | Save a BMP at every display update |

The sim runs the same LVGL UI code as the device; only the panel (an SDL window) and the Arduino/WiFi layers are stand-ins. Limitations: ePaper refresh time and ghosting are not modelled; WiFi and touch are not simulated.

## Layout

```
platformio.ini      environments: reterminal_e1001 (device), sim (SDL2)
include/lv_conf.h   LVGL configuration (device and sim)
src/
  main.cpp          setup/loop
  driver.h          Seeed_GFX board selection
  wifi_setup.*      saved-WiFi join, erase prompt, hotspot + QR codes
  config.*          settings table and NVS storage
  config_server.*   settings web page
  glucose.*         Nightscout / LibreLinkUp / Dexcom fetchers
  lvgl_port.*       LVGL <-> ePaper glue: flush callback (black/white + dither), full/partial refresh policy
  ui.*              LVGL helpers and the WiFi screens (QR codes, prompts)
  graph.*           glucose graph widget (line, low/high band, hour ticks)
  i18n.*            translation table (T("English text")), month names, language setting
  display.*         dashboard screen: mode switching, the grid of blocks
  cells.*           what a block can show (value, arrow, deltas, clock, age, freshness bar, graph)
  font_big_*.c      generated LVGL fonts for the glucose value (Montserrat Medium, digits only)
  font_ui_*.c       generated LVGL fonts for all other text (Montserrat Medium, ASCII + Latin-1)
  http.*            HTTP(S) client (device)
sim/
  sim_main.cpp      SDL2 host and drawing
  http_sim.cpp      HTTP client (libcurl)
  include/          Arduino, WiFi, WebServer, Preferences... stand-ins
tools/
  mock_cgm.py       mock CGM services for the simulator
```

## Notes

- Include order matters in `wifi_setup.cpp` and `config_server.cpp`: Seeed_GFX defines `FS_NO_GLOBALS`, which breaks `WebServer.h` if its headers come first.
- Button pins (GPIO 3 and 4, active low) come from the [Seeed Arduino peripherals cookbook](https://wiki.seeedstudio.com/reterminal_e10xx_with_arduino_peripherals/); confirm them on your board.
- The UI is LVGL 9.5 (pinned in `platformio.ini`; 9.6 changed some APIs). LVGL renders to RGB565, `src/lvgl_port.cpp` thresholds each pixel to black/white and writes it to the panel framebuffer. Only `UI_GREY` is dithered.
- `src/font_big_96.c` and `src/font_big_128.c` were generated from Montserrat Medium (SIL OFL) with `lv_font_conv --size 128 --bpp 4 --no-compress --format lvgl --symbols "0123456789.-+"`. Regenerate them the same way to change the size.
- **Languages.** The English text is the key of the translation table in `src/i18n.cpp`: `T("vs last")` returns the text in the chosen language, or the English text if there is no entry. To add a string, use `T()` where it is shown and add a row (fr, de, it, es) to the table; to add a language, add a column, a `Lang` value, a month row and an option in the `lang` field of `src/config.cpp`. Translations may only use ASCII and Latin-1 characters (accents of fr/de/it/es): that is what `font_ui_*.c` contains. For other scripts regenerate the fonts with more ranges, or with `--symbols`. The language is stored in flash like the other settings, so the WiFi screens at boot already use it (the very first boot is in English). Translations were written without a native-speaker review, corrections are welcome.
- `font_ui_14/16/24/32.c` were generated with `lv_font_conv --size N --bpp 4 --no-compress --format lvgl -r 0x20-0x7F -r 0xA0-0xFF` from Montserrat Medium (SIL OFL); `font_ui_48.c` has the ASCII range only because it only shows numbers.

## License

Copyright 2026 3djc. Licensed under the [PolyForm Noncommercial License 1.0.0](LICENSE): you may use, modify and share it for any **noncommercial** purpose, which includes personal use, hobby projects, research, education and use by charities and similar organizations. **Commercial use is not permitted** without a separate agreement with the copyright holder. Because of that restriction this is "source-available", not "open source" in the sense of the OSI definition.

Bundled third-party material keeps its own terms: LVGL, ArduinoJson and WiFiManager (MIT), Seeed_GFX (see its repository) and the Montserrat font used for the generated `font_*.c` files (SIL Open Font License 1.1). These are downloaded by PlatformIO at build time, except for the generated fonts that are part of this repository.
