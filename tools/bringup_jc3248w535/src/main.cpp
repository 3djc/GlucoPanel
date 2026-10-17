// JC3248W535 bring-up: display + backlight + touch + BOOT button. See platformio.ini.
//
// What it shows on the screen (landscape, 480x320): colour bars, a grey ramp, coloured corner markers to
// check orientation and mirroring, and live touch information. Serial (115200) gets the details: chip,
// memory, how long a full-frame push takes, raw touch bytes and the decoded tap / long-hold events.
#include <Arduino_GFX_Library.h>
#include <Wire.h>
#include <soc/rtc_cntl_reg.h>

// Pins (community-documented, to be confirmed by this test)
#define GFX_BL 1
#define TOUCH_ADDR 0x3B
#define TOUCH_SDA 4
#define TOUCH_SCL 8
#define TOUCH_RST_PIN 12
#define TOUCH_INT_PIN 11
#define BOOT_PIN 0

static const unsigned long LONG_HOLD_MS = 3000;

Arduino_ESP32QSPI* bus = new Arduino_ESP32QSPI(45, 47, 21, 48, 40, 39);
Arduino_AXS15231B* panel = new Arduino_AXS15231B(bus, GFX_NOT_DEFINED, 0, false, 320, 480);
Arduino_Canvas* gfx = new Arduino_Canvas(320, 480, panel, 0, 0, 0);

struct Touch {
  bool down = false;
  uint16_t x = 0, y = 0;     // screen coordinates (landscape)
  uint16_t rawX = 0, rawY = 0;
  uint8_t raw[8] = {};
};

// The controller is read with a command + 8 reply bytes: [gesture, points, x hi/lo, y hi/lo, ...].
static bool readTouch(Touch& t) {
  const uint8_t cmd[11] = {0xb5, 0xab, 0xa5, 0x5a, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00};
  Wire.beginTransmission(TOUCH_ADDR);
  Wire.write(cmd, sizeof(cmd));
  if (Wire.endTransmission() != 0) return false;
  if (Wire.requestFrom((int)TOUCH_ADDR, 8) != 8) return false;
  for (int i = 0; i < 8; i++) t.raw[i] = Wire.read();
  bool pressed = t.raw[1] > 0 && t.raw[1] <= 1;
  if (pressed) {
    t.rawX = ((t.raw[2] & 0x0F) << 8) | t.raw[3];
    t.rawY = ((t.raw[4] & 0x0F) << 8) | t.raw[5];
    if (t.rawX > 500 || t.rawY > 500) pressed = false;  // the controller reports garbage at the edges
  }
  if (pressed) {
    t.y = 320 - constrain(t.rawX, 0, 320);  // panel is portrait; rotation 1 = landscape
    t.x = t.rawY;
  }
  t.down = pressed;
  return true;
}

static void drawTestScreen() {
  gfx->fillScreen(RGB565_BLACK);
  const uint16_t bars[] = {RGB565_WHITE, RGB565_YELLOW, RGB565_CYAN, RGB565_GREEN, RGB565_MAGENTA, RGB565_RED, RGB565_BLUE, 0x7BEF};
  for (int i = 0; i < 8; i++) gfx->fillRect(i * 60, 0, 60, 56, bars[i]);
  for (int i = 0; i < 32; i++) {  // grey ramp: banding or wrong channel order would show here
    uint8_t v = i * 255 / 31;
    gfx->fillRect(i * 15, 56, 15, 24, gfx->color565(v, v, v));
  }
  // corner markers: TL red, TR green, BL blue, BR yellow (also catches mirroring)
  gfx->fillRect(0, 84, 28, 28, RGB565_RED);
  gfx->fillRect(452, 84, 28, 28, RGB565_GREEN);
  gfx->fillRect(0, 292, 28, 28, RGB565_BLUE);
  gfx->fillRect(452, 292, 28, 28, RGB565_YELLOW);
  gfx->setTextColor(RGB565_WHITE);
  gfx->setTextSize(2);
  gfx->setCursor(40, 90);
  gfx->print("JC3248W535 bring-up");
  gfx->setTextSize(1);
  gfx->setCursor(40, 116);
  gfx->print("480x320 landscape (rotation 1). Corners: TL red, TR green, BL blue, BR yellow.");
  gfx->setCursor(40, 130);
  gfx->print("Touch the screen: tap, or hold for 3 s.  BOOT button is logged on serial too.");
}

static const unsigned long RELEASE_AFTER_MS = 150;  // the controller reports a point once per scan, not on every read
static bool displayOk = false, touchAnswers = false;
static unsigned long firstFlushMs = 0, lastSeen = 0;
static unsigned long pressedAt = 0;
static bool longFired = false;
static uint32_t frames = 0;
static unsigned long lastInfo = 0, lastRaw = 0, lastFlushMs = 0;
static int lastBoot = HIGH;

static void showStatus(const char* line1, const char* line2) {
  gfx->fillRect(40, 150, 400, 130, RGB565_BLACK);
  gfx->setTextColor(RGB565_GREEN);
  gfx->setTextSize(2);
  gfx->setCursor(40, 160);
  gfx->print(line1);
  gfx->setCursor(40, 200);
  gfx->setTextColor(RGB565_CYAN);
  gfx->print(line2);
  gfx->setTextSize(1);
  gfx->setTextColor(RGB565_WHITE);
  gfx->setCursor(40, 250);
  gfx->printf("frames %lu   last flush %lu ms   up %lu s", (unsigned long)frames, lastFlushMs, millis() / 1000);
}

static void flushTimed() {
  unsigned long t0 = millis();
  gfx->flush();
  lastFlushMs = millis() - t0;
  frames++;
}

void setup() {
  Serial.begin(115200);
  delay(2000);  // time to attach the serial monitor
  Serial.println("\n=== JC3248W535 bring-up ===");
  Serial.printf("chip %s rev %d, %d cores, %lu MHz\n", ESP.getChipModel(), ESP.getChipRevision(), ESP.getChipCores(), (unsigned long)ESP.getCpuFreqMHz());
  Serial.printf("flash %lu MB, PSRAM %lu KB (free %lu KB), heap free %lu KB\n", (unsigned long)(ESP.getFlashChipSize() >> 20),
                (unsigned long)(ESP.getPsramSize() >> 10), (unsigned long)(ESP.getFreePsram() >> 10), (unsigned long)(ESP.getFreeHeap() >> 10));

  pinMode(BOOT_PIN, INPUT);  // plain input, no pull-up: GPIO0 is the strapping pin the USB auto-reset relies on
  pinMode(GFX_BL, OUTPUT);
  digitalWrite(GFX_BL, LOW);  // backlight off until the first frame is ready

  displayOk = gfx->begin();
  Serial.printf("display begin: %s\n", displayOk ? "ok" : "FAILED");
  gfx->setRotation(1);
  drawTestScreen();
  showStatus("Display ready", "touch me");
  flushTimed();
  digitalWrite(GFX_BL, HIGH);
  firstFlushMs = lastFlushMs;
  Serial.printf("first full-frame flush: %lu ms\n", lastFlushMs);

  Wire.begin(TOUCH_SDA, TOUCH_SCL);
  Wire.setClock(400000);
  pinMode(TOUCH_INT_PIN, INPUT_PULLUP);
  pinMode(TOUCH_RST_PIN, OUTPUT);
  digitalWrite(TOUCH_RST_PIN, LOW);
  delay(200);
  digitalWrite(TOUCH_RST_PIN, HIGH);
  delay(200);
  Wire.beginTransmission(TOUCH_ADDR);
  touchAnswers = Wire.endTransmission() == 0;
  Serial.printf("touch controller at 0x%02X: %s\n", TOUCH_ADDR, touchAnswers ? "answers" : "NO ANSWER");
}

// Serial command "B": restart into the ROM bootloader, so the next upload needs no buttons even if the
// automatic reset is not available.
static void checkSerialCommands() {
  while (Serial.available()) {
    int c = Serial.read();
    if (c == 'B' || c == 'b') {
      Serial.println("rebooting into the bootloader");
      Serial.flush();
      delay(100);
      REG_WRITE(RTC_CNTL_OPTION1_REG, RTC_CNTL_FORCE_DOWNLOAD_BOOT);
      esp_restart();
    }
  }
}

void loop() {
  checkSerialCommands();
  static Touch t;
  static bool wasDown = false;
  bool read = readTouch(t);
  unsigned long now = millis();

  if (read && t.down) {
    lastSeen = now;
    if (!wasDown) {
      wasDown = true;
      pressedAt = now;
      longFired = false;
      Serial.printf("TOUCH DOWN  screen %u,%u  raw %u,%u\n", t.x, t.y, t.rawX, t.rawY);
      char b[40];
      snprintf(b, sizeof(b), "down %u,%u", t.x, t.y);
      showStatus("Touch", b);
      gfx->fillCircle(t.x, t.y, 6, RGB565_WHITE);
      flushTimed();
    }
  }
  if (wasDown && !longFired && now - pressedAt >= LONG_HOLD_MS && now - lastSeen < RELEASE_AFTER_MS) {
    longFired = true;
    Serial.printf("LONG HOLD (%lu ms) at %u,%u\n", now - pressedAt, t.x, t.y);
    showStatus("LONG HOLD", "this would erase WiFi");
    flushTimed();
  }
  if (wasDown && now - lastSeen >= RELEASE_AFTER_MS) {  // no point seen for a while: released
    wasDown = false;
    unsigned long d = lastSeen - pressedAt;
    if (!longFired) {
      Serial.printf("TAP         screen %u,%u  held %lu ms\n", t.x, t.y, d);
      showStatus("TAP", "this would open settings");
    } else {
      Serial.printf("RELEASE after long hold (%lu ms)\n", d);
      showStatus("released", "after long hold");
    }
    flushTimed();
  }

  if (t.down && millis() - lastRaw > 500) {  // raw bytes while touching, to decode gestures or multi-touch later
    lastRaw = millis();
    Serial.printf("raw: %02X %02X %02X %02X %02X %02X %02X %02X\n", t.raw[0], t.raw[1], t.raw[2], t.raw[3], t.raw[4], t.raw[5], t.raw[6], t.raw[7]);
  }

  int boot = digitalRead(BOOT_PIN);
  if (boot != lastBoot) {
    lastBoot = boot;
    Serial.printf("BOOT button (GPIO%d): %s\n", BOOT_PIN, boot == LOW ? "pressed" : "released");
  }

  if (millis() - lastInfo > 5000) {  // heartbeat: proves the loop and the serial port are alive
    lastInfo = millis();
    Serial.printf("alive  up %lu s | display %s, first flush %lu ms, last flush %lu ms, frames %lu | touch %s | heap %lu KB, psram free %lu KB | INT %d | BOOT %d\n",
                  millis() / 1000, displayOk ? "ok" : "FAILED", firstFlushMs, lastFlushMs, (unsigned long)frames, touchAnswers ? "answers" : "NO ANSWER",
                  (unsigned long)(ESP.getFreeHeap() >> 10), (unsigned long)(ESP.getFreePsram() >> 10), digitalRead(TOUCH_INT_PIN), digitalRead(BOOT_PIN));
  }
  delay(5);
}
