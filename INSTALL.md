# Installing GlucoPanel

This guide takes you from a fresh device to a working glucose display. You flash a prebuilt release over USB; to build the firmware yourself, see [BUILD.md](BUILD.md).

Read the [medical disclaimer](README.md#medical-disclaimer) first. GlucoPanel is not a medical device.

## Supported devices

| Device | File name part |
| --- | --- |
| Seeed reTerminal **E1001** (7.5" e-paper, 800×480, black and white) | `reterminal-e1001` |
| Guition **JC3248W535** (3.5" colour touch LCD, 480×320) | `jc3248w535` |

Use the files that match your device. They are not interchangeable.

## What you need

- The device and a **USB data cable** (a charge-only cable will not show up as a serial port).
- A computer. To flash a release you need either [esptool](https://docs.espressif.com/projects/esptool/) (`pip install esptool`) or just Chrome / Edge (see the browser option below).
- A **2.4 GHz WiFi** network (the ESP32 does not do 5 GHz).
- An account on one of the supported sources: Dexcom Share, LibreLinkUp or Nightscout. For Dexcom and Libre, the sharing / follower feature must be set up.

## 1. Flash a release

Download the files for your device from the [latest release](https://github.com/3djc/GlucoPanel/releases/latest). Each device has two:

| File | Use it |
| --- | --- |
| `glucopanel-<device>-full.bin` | **First install.** Complete image (bootloader, partition table, app), flashed at `0x0`. It erases the saved settings. |
| `glucopanel-<device>-update.bin` | **Update.** The app only, flashed at `0x10000`. Saved WiFi and settings are kept. |

`<device>` is `reterminal-e1001` or `jc3248w535`. `SHA256SUMS.txt` lists the checksums; verify them with `shasum -a 256 -c SHA256SUMS.txt`.

Plug the device in, then flash with esptool (v5; with v4 write `write_flash`):

```sh
# First install
esptool --chip esp32s3 write-flash 0x0 glucopanel-jc3248w535-full.bin

# Later update
esptool --chip esp32s3 write-flash 0x10000 glucopanel-jc3248w535-update.bin
```

esptool finds the port by itself; if it finds several or none, add `--port /dev/cu.usbmodem1101` (macOS), `/dev/ttyACM0` (Linux) or `COM5` (Windows).

**In the browser instead:** Chrome or Edge can flash over USB with a web tool such as [Adafruit's WebSerial ESPTool](https://adafruit.github.io/Adafruit_WebSerial_ESPTool/): connect, choose the `-full.bin` file at offset `0x0`, and program. Nothing needs installing.

When it finishes, unplug and replug the device (or press reset) and go on to step 2.

## If flashing cannot connect

esptool resets the chip into its bootloader automatically. If it still fails ("Failed to connect", "No serial data received"), put the chip in download mode by hand and flash again:

1. Unplug the USB cable.
2. Hold the **BOOT** button (on the JC3248W535, one of the two small buttons on the back; on the reTerminal, see its documentation).
3. Plug the cable back in while holding BOOT, then release it.
4. Run the flash command.
5. Unplug and replug the cable **without** BOOT held, so the new firmware starts.

A chip left in the bootloader shows a blank screen; a plain replug fixes it.

## Optional: back up what is on the device

If the board already runs something you want to keep, copy its flash before you overwrite it (the JC3248W535 has 16 MB):

```sh
esptool --chip esp32s3 read-flash 0 0x1000000 backup.bin
```

The backup can contain saved WiFi and account data, so keep it private.

## 2. First start: connect it to your WiFi

1. The device boots and has no WiFi yet, so it opens its own hotspot named **`GlucoPanel`**. The screen shows two QR codes and the hotspot password (random, made on first use).
2. Scan the **first QR code** with your phone to join the hotspot (or join it by hand with the password shown).
3. Scan the **second QR code**, or open `http://192.168.4.1`, to get the WiFi setup page. Pick your network, enter its password and save.
4. The device restarts and joins your network. The screen now shows the network name, its IP address and a QR code for the settings page.

## 3. Configure the glucose source

1. Scan the QR code on the screen, or open `http://glucopanel.local` (or the IP address shown) from a device on the same network. The QR code already contains the 6-digit access code; if you type the address by hand, enter the code shown on the device.
2. Choose your **language**, the **data source**, and enter the credentials:
   - **Dexcom Share:** your Dexcom username (not a phone number or email) and password, and your region.
   - **LibreLinkUp:** your LibreLinkUp (follower) email and password.
   - **Nightscout:** the site URL and, if needed, an access token.
3. Set the unit (mg/dL or mmol/L), the low and high limits, your time zone, and what each block of the screen shows.
4. Save. The first reading appears within a minute.

The full list of settings is in the [README](README.md#settings-page).

## Everyday use

| | reTerminal E1001 | JC3248W535 |
| --- | --- | --- |
| Show the settings QR code and a new code | Short press the green button (KEY0) | Tap the screen |
| Erase the saved WiFi and start setup again | Hold the green button for 3 s | Open the settings screen, hold "Hold 3 s to erase the WiFi" |

If the saved WiFi cannot be joined at boot, the device asks whether to erase it or retry; it retries by itself after 60 s.

## Updating

**Automatically.** A panel running this version or later updates itself: once a day it looks for a newer release on GitHub and installs it at night (03:00 by default), keeping your settings. Nothing to do, and you can switch it off or install at once in the settings page ([README](README.md#firmware-updates)). The first install of such a version has to be done over USB; versions older than that cannot update themselves.

**Over USB.** Flash the `-update.bin` of the new release at `0x10000`. Your settings live in a separate flash area and are kept. (Flashing a `-full.bin` erases them.)

## Troubleshooting

- **No serial port appears.** Try another cable (it must carry data) and another USB port, then use the download mode steps above.
- **Blank screen after flashing.** Unplug and replug the cable. A chip left in download mode does not run the firmware.
- **Cannot reach `glucopanel.local`.** Some networks and Windows setups do not resolve `.local` names; use the IP address shown on the device.
- **"Certificate verification failed" shown as an error.** The device checks HTTPS servers against its built-in root list and needs the correct time, which it gets over NTP. If your Nightscout uses a self-signed certificate, set "Check server certificates" to No in the settings.
- **Forgot the settings code or lost the page.** Short press the green button, or tap the screen, to get a new code and QR code.
- **The panel does not update.** Open the settings page: the **Firmware updates** block shows the version and the result of the last check (a failure says why, for example the clock is not set yet or GitHub cannot be reached). Updates need the panel to be on at the install hour and no fetch error showing then. **Check for updates** and **Install now** work at any time.
- **Start over completely.** Erase the WiFi as above; to wipe everything, run `esptool --chip esp32s3 erase-flash` and flash the `-full.bin` again.
