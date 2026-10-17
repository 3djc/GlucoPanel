// SDL-backed stand-in for Seeed_GFX's EPaper class (the subset the LVGL port uses).
// Pixel values follow the E1001 config: TFT_WHITE = 0x0, TFT_BLACK = 0xF.
#pragma once
#include <Arduino.h>

#define TFT_BLACK 0xF
#define TFT_WHITE 0x0

#define SIM_W 800
#define SIM_H 480

class EPaper {
 public:
  void begin();
  void update();  // full refresh: present framebuffer in the SDL window
  void updataPartial(uint16_t x, uint16_t y, uint16_t w, uint16_t h);  // partial refresh of a window
  void fillScreen(uint16_t c);
  void drawPixel(int x, int y, uint16_t c);

 private:
  void present();
  uint8_t fb_[SIM_W * SIM_H];
};
