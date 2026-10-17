// Native SDL2 host: runs the firmware's setup()/loop() and shows the "ePaper".
// SIM_SCREENSHOT=file%d.bmp saves each update(); SIM_FRAMES=n exits after n loop() calls.
#include <SDL.h>
#include <Arduino.h>
#include <TFT_eSPI.h>
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
