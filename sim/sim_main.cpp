// Native SDL2 host: runs the firmware's setup()/loop() and shows the "ePaper".
// SIM_SCREENSHOT=file%d.bmp saves each update(); SIM_FRAMES=n exits after n loop() calls.
#include <SDL.h>
#include <Arduino.h>
#ifdef UI_PROFILE_LCD
#include "panel.h"
#include "touch.h"
#else
#include <TFT_eSPI.h>
#endif
#include <cstdlib>

SerialShim Serial;
unsigned long millis() { return SDL_GetTicks(); }
static void pump();
void delay(unsigned long ms) { SDL_Delay((Uint32)ms); pump(); }

static SDL_Window* win;
static SDL_Renderer* ren;
static SDL_Texture* tex;
static bool quit_requested;

bool sim_wifi_saved = getenv("SIM_WIFI") && !strcmp(getenv("SIM_WIFI"), "fail");
bool sim_wifi_connected = false;

void pinMode(int, int) {}
int digitalRead(int pin) {
  pump();
  // SIM_TAP=<pin>:<start ms>:<duration ms>[,...] presses that button during each window (scripted tests)
  if (const char* taps = getenv("SIM_TAP")) {
    const char* p = taps;
    while (*p) {
      int tp = 0;
      long start = 0, dur = 0;
      if (sscanf(p, "%d:%ld:%ld", &tp, &start, &dur) == 3 && tp == pin && (long)millis() >= start && (long)millis() < start + dur)
        return LOW;
      const char* comma = strchr(p, ',');
      if (!comma) break;
      p = comma + 1;
    }
  }
  if (getenv("SIM_PRESS") && atoi(getenv("SIM_PRESS")) == pin) return LOW;
  const Uint8* k = SDL_GetKeyboardState(nullptr);
  SDL_Scancode sc = pin == 3 ? SDL_SCANCODE_1 : pin == 4 ? SDL_SCANCODE_2 : SDL_SCANCODE_3;
  return k[sc] ? LOW : HIGH;
}

bool sim_wait_enter() {
  if (getenv("SIM_NOWAIT")) return true;
  while (!quit_requested) {
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
      if (e.type == SDL_QUIT || (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_ESCAPE))
        quit_requested = true;
      if (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_RETURN) return true;
    }
    SDL_Delay(10);
  }
  return false;
}

static void pump() {
  SDL_Event e;
  while (SDL_PollEvent(&e)) {
    if (e.type == SDL_QUIT || (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_ESCAPE))
      exit(0);  // window closed / Esc, even while the firmware is blocked in a wait loop
  }
}

#ifndef UI_PROFILE_LCD
void EPaper::begin() {
  SDL_Init(SDL_INIT_VIDEO);
  win = SDL_CreateWindow("Gluco Panel (sim)", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                         SIM_W, SIM_H, SDL_WINDOW_ALLOW_HIGHDPI);
  ren = SDL_CreateRenderer(win, -1, 0);
  tex = SDL_CreateTexture(ren, SDL_PIXELFORMAT_RGB24, SDL_TEXTUREACCESS_STREAMING, SIM_W, SIM_H);
  fillScreen(TFT_WHITE);
}

void EPaper::update() {
  static int fullCount = 0;
  printf("epaper: FULL refresh #%d\n", ++fullCount);
  present();
}

void EPaper::updataPartial(uint16_t x, uint16_t y, uint16_t w, uint16_t h) {
  static int partialCount = 0;
  printf("epaper: partial refresh #%d (%d,%d %dx%d)\n", ++partialCount, x, y, w, h);
  present();
}

void EPaper::present() {
  static uint8_t rgb[SIM_W * SIM_H * 3];
  for (int i = 0; i < SIM_W * SIM_H; i++) {
    uint8_t g = 255 - fb_[i] * 17;  // 0 -> white, 15 -> black
    rgb[i * 3] = rgb[i * 3 + 1] = rgb[i * 3 + 2] = g;
  }
  SDL_UpdateTexture(tex, nullptr, rgb, SIM_W * 3);
  SDL_RenderClear(ren);
  SDL_RenderCopy(ren, tex, nullptr, nullptr);
  SDL_RenderPresent(ren);
  if (const char* path = getenv("SIM_SCREENSHOT")) {
    SDL_Surface* s = SDL_CreateRGBSurfaceWithFormatFrom(rgb, SIM_W, SIM_H, 24, SIM_W * 3,
                                                        SDL_PIXELFORMAT_RGB24);
    static int n = 0;
    char name[512];
    snprintf(name, sizeof(name), path, ++n);  // path may contain %d
    SDL_SaveBMP(s, name);
    SDL_FreeSurface(s);
  }
  pump();
}

void EPaper::fillScreen(uint16_t c) { memset(fb_, c & 0xF, sizeof(fb_)); }
void EPaper::drawPixel(int x, int y, uint16_t c) {
  if (x >= 0 && y >= 0 && x < SIM_W && y < SIM_H) fb_[y * SIM_W + x] = c & 0xF;
}
#else
// ---- colour LCD stand-in (JC3248W535): an SDL window, the mouse is the touch screen ----
static const int LCD_W = 480, LCD_H = 320;
static uint16_t lcdFb[LCD_W * LCD_H];
static int lcdBacklight = 100;  // percent; the sim dims the picture to show it

void panelBegin() {
  SDL_Init(SDL_INIT_VIDEO);
  win = SDL_CreateWindow("Gluco Panel (sim, LCD)", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, LCD_W, LCD_H, SDL_WINDOW_ALLOW_HIGHDPI);
  ren = SDL_CreateRenderer(win, -1, 0);
  tex = SDL_CreateTexture(ren, SDL_PIXELFORMAT_RGB565, SDL_TEXTUREACCESS_STREAMING, LCD_W, LCD_H);
  memset(lcdFb, 0xFF, sizeof(lcdFb));
}

void panelDraw(int x, int y, int w, int h, const uint16_t* px) {
  for (int j = 0; j < h; j++) {
    if (y + j < 0 || y + j >= LCD_H) continue;
    for (int i = 0; i < w; i++)
      if (x + i >= 0 && x + i < LCD_W) lcdFb[(y + j) * LCD_W + x + i] = px[j * w + i];
  }
}

void panelPresent() {
  static int n = 0;
  printf("lcd: frame #%d\n", ++n);
  static uint16_t shownFb[LCD_W * LCD_H];
  for (int i = 0; i < LCD_W * LCD_H; i++) {
    uint16_t c = lcdFb[i];
    int r = ((c >> 11) & 0x1F) * lcdBacklight / 100, g = ((c >> 5) & 0x3F) * lcdBacklight / 100, b = (c & 0x1F) * lcdBacklight / 100;
    shownFb[i] = (r << 11) | (g << 5) | b;
  }
  SDL_UpdateTexture(tex, nullptr, shownFb, LCD_W * 2);
  SDL_RenderClear(ren);
  SDL_RenderCopy(ren, tex, nullptr, nullptr);
  SDL_RenderPresent(ren);
  if (const char* path = getenv("SIM_SCREENSHOT")) {
    static uint8_t rgb[LCD_W * LCD_H * 3];
    for (int i = 0; i < LCD_W * LCD_H; i++) {
      uint16_t c = shownFb[i];
      rgb[i * 3] = ((c >> 11) & 0x1F) * 255 / 31;
      rgb[i * 3 + 1] = ((c >> 5) & 0x3F) * 255 / 63;
      rgb[i * 3 + 2] = (c & 0x1F) * 255 / 31;
    }
    SDL_Surface* sf = SDL_CreateRGBSurfaceWithFormatFrom(rgb, LCD_W, LCD_H, 24, LCD_W * 3, SDL_PIXELFORMAT_RGB24);
    static int shot = 0;
    char name[512];
    snprintf(name, sizeof(name), path, ++shot);
    SDL_SaveBMP(sf, name);
    SDL_FreeSurface(sf);
  }
  pump();
}

void panelBacklight(int percent) {
  lcdBacklight = percent < 0 ? 0 : percent > 100 ? 100 : percent;
  printf("lcd: backlight %d%%\n", lcdBacklight);
  panelPresent();  // show the dimmed picture
}

void touchBegin() {}

// SIM_TOUCH=<x>:<y>:<start ms>:<duration ms>[,...] touches that point during each window (scripted tests)
TouchPoint touchPoll() {
  TouchPoint t;
  pump();
  if (const char* taps = getenv("SIM_TOUCH")) {
    const char* p = taps;
    while (*p) {
      int x = 0, y = 0;
      long start = 0, dur = 0;
      if (sscanf(p, "%d:%d:%ld:%ld", &x, &y, &start, &dur) == 4 && (long)millis() >= start && (long)millis() < start + dur) {
        t.down = true;
        t.x = x;
        t.y = y;
        return t;
      }
      const char* comma = strchr(p, ',');
      if (!comma) break;
      p = comma + 1;
    }
  }
  int mx = 0, my = 0;
  if (SDL_GetMouseState(&mx, &my) & SDL_BUTTON(SDL_BUTTON_LEFT)) {
    t.down = true;
    t.x = mx;
    t.y = my;
  }
  return t;
}
#endif

int main() {
  setvbuf(stdout, nullptr, _IOLBF, 0);  // line-buffered so logs show up when redirected
  setup();
  long frames = getenv("SIM_FRAMES") ? atol(getenv("SIM_FRAMES")) : -1;
  while (!quit_requested && frames != 0) {
    loop();
    pump();
    SDL_Delay(1);
    if (frames > 0) frames--;
  }
  SDL_Quit();
  return 0;
}
