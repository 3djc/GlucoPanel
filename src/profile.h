#pragma once
#include <lvgl.h>

// Target profiles. The default is the e-paper reTerminal E1001 (800x480, black and white); building with
// -DUI_PROFILE_LCD selects the colour LCD of the Guition JC3248W535 (480x320, touch). Every size that depends
// on the screen lives in one table per profile (profile.cpp), so the drawing code has no screen-specific
// numbers. The e-paper values were measured on the rendered screen; the LCD ones are tuned in the simulator.
struct Metrics {
  int screenW, screenH;
  // dashboard rows: the top row (the band), the middle row and the graph. On the e-paper they tile the screen
  // exactly: the bottom of one row is the top of the next.
  int bandX, bandY, bandW, bandH, bandRadius, bandSepInset;
  int midY, midH;
  int graphY, graphH;
  int stripY, edge;  // error text (over the bottom of the graph on the e-paper), and its left margin
  // blocks: their content is centred in the block from the fonts' real glyph sizes (see cells.cpp)
  int tallH;         // a block at least this tall gets the big value font and the taller freshness bar
  int arrowBox, arrowLen;
  int barTall, barShort, segW, segGap, segBorder, segRadius;  // freshness bar
  // graph
  int gLeft, gRight, gTop, gBottom;  // margins inside the widget (y labels left, hour labels bottom)
  int gLine, gDot, gDotBig, gTick, gLabelH, gTickLabelHalfW, gTickLabelDy, gDash;
  // fonts
  const lv_font_t *fSmall, *fCaption, *fMid, *fTitle, *fNum, *fBigL, *fBigM;
  const lv_font_t* fNumBig;  // the numbers of the blocks when they fit; fNum otherwise (same as fNum on the LCD)
  int qr;  // size of the QR codes on the WiFi and settings screens
};

extern const Metrics M;

// The WiFi and settings screens are laid out in "design" coordinates (the 800x480 e-paper layout) and
// scaled to the screen.
inline int dx(int v) { return v * M.screenW / 800; }
inline int dy(int v) { return v * M.screenH / 480; }
