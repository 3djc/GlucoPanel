// Native SDL2 host: runs the firmware's setup()/loop() and shows the "ePaper".
// SIM_SCREENSHOT=file%d.bmp saves each update(); SIM_FRAMES=n exits after n loop() calls.
#include <SDL.h>
#include <Arduino.h>
#include <TFT_eSPI.h>
#include <algorithm>
#include <cstdlib>
#include <cmath>
#include "glcdfont.h"

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

void EPaper::begin() {
  SDL_Init(SDL_INIT_VIDEO);
  win = SDL_CreateWindow("Gluco Panel (sim)", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                         SIM_W, SIM_H, SDL_WINDOW_ALLOW_HIGHDPI);
  ren = SDL_CreateRenderer(win, -1, 0);
  tex = SDL_CreateTexture(ren, SDL_PIXELFORMAT_RGB24, SDL_TEXTUREACCESS_STREAMING, SIM_W, SIM_H);
  fillScreen(TFT_WHITE);
}

void EPaper::update() {
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
void EPaper::fillRect(int x, int y, int w, int h, uint16_t c) {
  for (int j = y; j < y + h; j++)
    for (int i = x; i < x + w; i++) drawPixel(i, j, c);
}
void EPaper::drawRect(int x, int y, int w, int h, uint16_t c) {
  fillRect(x, y, w, 1, c); fillRect(x, y + h - 1, w, 1, c);
  fillRect(x, y, 1, h, c); fillRect(x + w - 1, y, 1, h, c);
}
void EPaper::drawLine(int x0, int y0, int x1, int y1, uint16_t c) {
  int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1, dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
  int err = dx + dy;
  for (;;) {
    drawPixel(x0, y0, c);
    if (x0 == x1 && y0 == y1) break;
    int e2 = 2 * err;
    if (e2 >= dy) { err += dy; x0 += sx; }
    if (e2 <= dx) { err += dx; y0 += sy; }
  }
}
void EPaper::drawCircle(int cx, int cy, int r, uint16_t c) {
  for (int a = 0; a < 720; a++)
    drawPixel(cx + (int)lround(r * cos(a * M_PI / 360)), cy + (int)lround(r * sin(a * M_PI / 360)), c);
}
void EPaper::fillCircle(int cx, int cy, int r, uint16_t c) {
  for (int j = -r; j <= r; j++)
    for (int i = -r; i <= r; i++)
      if (i * i + j * j <= r * r) drawPixel(cx + i, cy + j, c);
}

void EPaper::fillTriangle(int x0, int y0, int x1, int y1, int x2, int y2, uint16_t c) {
  int minx = std::min({x0, x1, x2}), maxx = std::max({x0, x1, x2});
  int miny = std::min({y0, y1, y2}), maxy = std::max({y0, y1, y2});
  auto edge = [](int ax, int ay, int bx, int by, int px, int py) { return (bx - ax) * (py - ay) - (by - ay) * (px - ax); };
  for (int y = miny; y <= maxy; y++)
    for (int x = minx; x <= maxx; x++) {
      int a = edge(x0, y0, x1, y1, x, y), b = edge(x1, y1, x2, y2, x, y), d = edge(x2, y2, x0, y0, x, y);
      if ((a >= 0 && b >= 0 && d >= 0) || (a <= 0 && b <= 0 && d <= 0)) drawPixel(x, y, c);
    }
}

int EPaper::drawString(const String& s, int x, int y, int font) {
  static const int scaleFor[] = {1, 1, 2, 2, 3, 3, 5, 6, 8};  // by TFT_eSPI font number
  int scale = (font >= 0 && font <= 8 ? scaleFor[font] : 1) * size_;
  int cx = x;
  for (unsigned char ch : s) {
    for (int col = 0; col < 5; col++) {
      uint8_t bits = font_[ch * 5 + col];
      for (int row = 0; row < 7; row++)
        if (bits & (1 << row)) fillRect(cx + col * scale, y + row * scale, scale, scale, fg_);
    }
    cx += 6 * scale;
  }
  return cx - x;
}

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
