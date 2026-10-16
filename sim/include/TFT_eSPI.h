// SDL-backed stand-in for Seeed_GFX's EPaper class (subset of the API).
// Pixel values follow the E1001 config: TFT_WHITE = 0x0, TFT_BLACK = 0xF.
#pragma once
#include <Arduino.h>

#define TFT_BLACK 0xF
#define TFT_WHITE 0x0
#define TFT_DARKGREY 0xA
#define TFT_LIGHTGREY 0x5

#define SIM_W 800
#define SIM_H 480

class EPaper {
 public:
  void begin();
  void update();  // present framebuffer in the SDL window
  void sleep() {}
  int width() const { return SIM_W; }
  int height() const { return SIM_H; }

  void fillScreen(uint16_t c);
  void drawPixel(int x, int y, uint16_t c);
  void drawLine(int x0, int y0, int x1, int y1, uint16_t c);
  void drawFastHLine(int x, int y, int w, uint16_t c) { fillRect(x, y, w, 1, c); }
  void drawFastVLine(int x, int y, int h, uint16_t c) { fillRect(x, y, 1, h, c); }
  void drawRect(int x, int y, int w, int h, uint16_t c);
  void fillRect(int x, int y, int w, int h, uint16_t c);
  void fillTriangle(int x0, int y0, int x1, int y1, int x2, int y2, uint16_t c);
  void drawCircle(int cx, int cy, int r, uint16_t c);
  void fillCircle(int cx, int cy, int r, uint16_t c);

  void setTextColor(uint16_t c) { fg_ = c; }
  void setTextSize(int s) { size_ = s < 1 ? 1 : s; }
  // Same glyph data and metrics as Seeed_GFX fonts 1, 2, 4, 6, 7 and 8 (top-left datum).
  int drawString(const String& s, int x, int y, int font = 1);
  int textWidth(const String& s, int font = 1);
  int fontHeight(int font = 1);

 private:
  uint8_t fb_[SIM_W * SIM_H];
  uint16_t fg_ = TFT_BLACK;
  int size_ = 1;
};
