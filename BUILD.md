# Building GlucoPanel from source

Most people should flash a [prebuilt release](INSTALL.md). Build from source if you want to change the code or try a branch.

## Requirements

- [PlatformIO](https://platformio.org/install) (the VS Code extension or the `pio` command line) and Git.
- For a device: a USB data cable. Flashing steps that do not depend on how the firmware was built (download mode, backup, first start) are in [INSTALL.md](INSTALL.md).

## Build and flash

The first build downloads the platform, the toolchain and the libraries, so it takes a few minutes.

```sh
git clone git@github.com:3djc/GlucoPanel.git
cd GlucoPanel

pio run -e reterminal_e1001               # build for the reTerminal E1001
pio run -e reterminal_e1001 -t upload     # build and flash

pio run -e jc3248w535                     # build for the JC3248W535
pio run -e jc3248w535 -t upload           # build and flash

pio device monitor                        # serial output, 115200 baud
```

PlatformIO finds the serial port by itself; pass `--upload-port` if it does not. If the upload cannot connect, put the chip in download mode by hand (see [INSTALL.md](INSTALL.md#if-flashing-cannot-connect)).

## Notes for the JC3248W535

- It needs the pioarduino platform (Arduino core 3.x) and GFX Library for Arduino **1.6.9**; older versions do not compile against core 3.3 (error about `spiFrequencyToClockDiv`). Do not change them in `platformio.ini`.
- The environment sets `ARDUINO_USB_MODE=1`, so the chip's own USB-Serial-JTAG is used. With Arduino's TinyUSB serial instead, `esptool` cannot reset the chip and every upload would need the BOOT button.
- `tools/bringup_jc3248w535/` is a small standalone test (colour bars, touch coordinates, a `B` serial command that reboots into the bootloader).
- The display is pushed as a full frame (~50 ms); some batches of this panel ignore partial window updates. Touch is read over I2C (address 0x3B); a touch counts as released after 150 ms without a point.
- Pins: display QSPI 45, 47, 21, 48, 40, 39, backlight 1, touch SDA 4, SCL 8, INT 11, RST 12.

`src/driver.h` selects the board and screen for the Seeed_GFX library (`BOARD_SCREEN_COMBO 520` = reTerminal E1001). For other boards see the [Seeed ePaper + PlatformIO guide](https://wiki.seeedstudio.com/epaper_work_with_platformio/).

## Simulator

The `sim` environment builds the same `src/` code for your computer, with an SDL2 window standing in for the ePaper and a small HTTP server standing in for the device's web server.

```sh
brew install sdl2            # macOS, once
pio run -e sim -t exec        # e-paper profile (800x480)
pio run -e sim_lcd -t exec    # colour LCD profile (480x320, click = touch)
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
| `SIM_TOUCH=<x>:<y>:<start ms>:<duration ms>[,...]` | LCD simulator: touch that point during each time window (the mouse also works) |
| `SIM_TAP=<pin>:<start ms>:<duration ms>[,...]` | Press that button during each time window, e.g. `3:4000:300` is a short press at 4 s, `3:3000:4500` a long one |
| `SIM_FRAMES=<n>` | Exit after `n` calls to `loop()` |
| `SIM_SCREENSHOT=out_%d.bmp` | Save a BMP at every display update |

The sim runs the same LVGL UI code as the device; only the panel (an SDL window) and the Arduino/WiFi layers are stand-ins. Limitations: ePaper refresh time and ghosting are not modelled; WiFi and touch are not simulated.

## Source layout

```
platformio.ini      environments: reterminal_e1001, jc3248w535 (devices), sim, sim_lcd (SDL2)
include/lv_conf.h   LVGL configuration (device and sim)
src/
  main.cpp          setup/loop
  driver.h          Seeed_GFX board selection
  wifi_setup.*      saved-WiFi join, erase prompt, hotspot + QR codes
  config.*          settings table and NVS storage
  config_server.*   settings web page
  glucose.*         Nightscout / LibreLinkUp / Dexcom fetchers
  profile.*         per-profile table of every screen-dependent size and font (e-paper / LCD)
  lvgl_port.*       LVGL <-> panel glue: e-paper: black/white + dither, full/partial refresh policy; LCD: frame push
  panel.h, panel_axs15231b.cpp   LCD panel interface and the JC3248W535 driver (QSPI, backlight)
  touch.h, touch_axs15231b.cpp   touch interface and the JC3248W535 driver (I2C)
  input.*           one input layer: buttons (e-paper) or touch (LCD) -> tap / hold
  ui.*              LVGL helpers and the WiFi screens (QR codes, prompts)
  graph.*           glucose graph widget (line, low/high band, hour ticks)
  i18n.*            translation table (T("English text")), month names, language setting
  display.*         dashboard screen: mode switching, the grid of blocks
  cells.*           what a block can show (value, arrow, deltas, clock, age, freshness bar, graph)
  font_big_*.c      generated LVGL fonts for the glucose value (Montserrat Medium, digits only)
  font_ui_*.c       generated LVGL fonts for all other text (Montserrat Medium, ASCII + Latin-1)
  font_lcd_*.c      the same for the 480x320 colour profile (smaller sizes)
  http.*            HTTP(S) client (device)
sim/
  sim_main.cpp      SDL2 host: e-paper window, or the colour LCD window with the mouse as touch screen
  http_sim.cpp      HTTP client (libcurl)
  include/          Arduino, WiFi, WebServer, Preferences... stand-ins
tools/
  mock_cgm.py       mock CGM services for the simulator
```

## Development notes

- Include order matters in `wifi_setup.cpp` and `config_server.cpp`: Seeed_GFX defines `FS_NO_GLOBALS`, which breaks `WebServer.h` if its headers come first.
- Button pins (GPIO 3 and 4, active low) come from the [Seeed Arduino peripherals cookbook](https://wiki.seeedstudio.com/reterminal_e10xx_with_arduino_peripherals/); confirm them on your board.
- The UI is LVGL 9.5 (pinned in `platformio.ini`; 9.6 changed some APIs). LVGL renders to RGB565, `src/lvgl_port.cpp` thresholds each pixel to black/white and writes it to the panel framebuffer. Only `UI_GREY` is dithered.
- `src/font_big_96.c` and `src/font_big_128.c` were generated from Montserrat Medium (SIL OFL) with `lv_font_conv --size 128 --bpp 4 --no-compress --format lvgl --symbols "0123456789.-+"`. Regenerate them the same way to change the size.
- **Languages.** The English text is the key of the translation table in `src/i18n.cpp`: `T("vs last")` returns the text in the chosen language, or the English text if there is no entry. To add a string, use `T()` where it is shown and add a row (fr, de, it, es) to the table; to add a language, add a column, a `Lang` value, a month row and an option in the `lang` field of `src/config.cpp`. Translations may only use ASCII and Latin-1 characters (accents of fr/de/it/es): that is what `font_ui_*.c` contains. For other scripts regenerate the fonts with more ranges, or with `--symbols`. The language is stored in flash like the other settings, so the WiFi screens at boot already use it (the very first boot is in English). Translations were written without a native-speaker review, corrections are welcome.
- `font_ui_14/16/24/32.c` were generated with `lv_font_conv --size N --bpp 4 --no-compress --format lvgl -r 0x20-0x7F -r 0xA0-0xFF` from Montserrat Medium (SIL OFL); `font_ui_48.c` has the ASCII range only because it only shows numbers.

## E-paper refresh policy

Full refreshes flash and wear the panel, so `src/lvgl_port.cpp` and `src/display.cpp` avoid them:

- **Full refresh** only when the screen layout changes (not configured / no data / reading, or the source or unit changes) and every 30 minutes to clear ghosting (`FULL_REFRESH_MS`).
- **Partial refresh** of one window covering just the blocks that changed (arrow, deltas, value), the "Updated N min ago" strip once a minute, and the error text.
- **No refresh** when a fetch changes nothing, e.g. repeated identical errors.

The simulator logs every refresh (`epaper: FULL refresh` / `partial refresh`) so you can check this.

The E1001's real 4-level gray mode has no partial refresh, so the light grey (`UI_GREY`) is a sparse black/white dot pattern: `LIGHT_GREY_DOTS` in `src/lvgl_port.cpp`, 0 to 16 black dots per 4×4 pixels.

## Data sources

| Source | How it is read |
| --- | --- |
| Nightscout | `GET <url>/api/v1/entries/sgv.json?count=1`, optional `&token=` (an access token; the raw API secret is not supported) |
| LibreLinkUp | `POST /llu/auth/login` (follows region redirects), then `GET /llu/connections` with `Authorization` and the SHA-256 `account-id` header; uses the first connection |
| Dexcom Share | `AuthenticatePublisherAccount`, `LoginPublisherAccountById`, then `ReadPublisherLatestGlucoseValues`; uses a username (not a phone/email login) |

LibreLinkUp and Dexcom Share are unofficial APIs and can change without notice. Sessions are cached and renewed on expiry. The clock is set over NTP because reading ages need it.

**Server certificates.** HTTPS connections are checked against the root certificate list of the ESP32 SDK (the Mozilla list, about 68 KB, already in the firmware; `src/http.cpp`), so a hostile network cannot pose as Dexcom, LibreLinkUp or your Nightscout and read your credentials. Certificates have validity dates, so this needs the clock to be right; the panel waits for NTP before the first fetch. If your Nightscout runs on its own server with a **self-signed certificate**, set "Check server certificates" to No in the settings (this disables the check for every service, so prefer a normal certificate, or plain `http://` on a network you trust). Dexcom Share and all LibreLinkUp regions chain to roots in that list (GTS Root R4 / GlobalSign Root CA); this was not tested on a real device. A failed check shows as a fetch error such as "X509 - Certificate verification failed".

## Data and settings internals

- Saved settings live in the ESP32's flash (NVS); read them with `cfgGet("key")`.
- History in RAM: the last `GRAPH_HOURS` = 3 h for the graph (from Dexcom and Nightscout; LibreLinkUp via its graph endpoint) and `STATS_HOURS` = 24 h for the statistics. The refresh interval comes from `refreshMinutes()`.
- **HbA1c.** Two weeks of readings are far more than the RAM keeps, so every reading is also added to per-day totals (seconds covered and glucose × seconds, 30 days, a few hundred bytes of text) saved in NVS under the `hba1c` key every 30 min. The totals are tied to an account hash and discarded when the data source or its credentials (including the region) change.

## Making a release

Pushing a tag such as `v1.0.0` runs `.github/workflows/release.yml`, which builds both environments and publishes the images on GitHub.

```sh
git tag v1.0.0 && git push --tags
```

`tools/package_release.sh <env> <name> <outdir>` makes the same two files locally after a `pio run` (it needs `pip install esptool`):

- `<name>-full.bin`: bootloader, partition table and app merged, flashed at `0x0` (first install, erases settings).
- `<name>-update.bin`: the app alone, flashed at `0x10000` (keeps settings).

The workflow can also be started by hand from the Actions tab to check that the build works without publishing.
