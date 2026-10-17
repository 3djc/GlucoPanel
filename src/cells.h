#pragma once
#include <Arduino.h>
#include <lvgl.h>
#include "glucose.h"

// The dashboard is made of blocks; each block shows one of these, chosen in the settings page
// (see the "block_*" fields in config.cpp and CELL_OPTIONS there). Small blocks are ~266 px wide and
// 130..160 px tall; the graph is made for the full-width bottom block.
enum CellType {
  CELL_NONE,
  CELL_VALUE,      // the glucose value, big, with its unit
  CELL_ARROW,      // trend arrow
  CELL_DELTA1,     // change vs the previous reading
  CELL_DELTA15,    // change vs 15 min ago
  CELL_CLOCK,      // current date and time
  CELL_READTIME,   // time the reading was taken
  CELL_AGE,        // how old the reading is, in minutes
  CELL_FRESH,      // freshness bar: one segment per minute of age, up to 5
  CELL_AVG,        // average glucose over the collected history (up to 24 h)
  CELL_TIR,        // time in range (between the low and high limits), with time below / above
  CELL_RANGE,      // lowest and highest reading
  CELL_CV,         // glucose variability: coefficient of variation, with the standard deviation
  CELL_GRAPH,      // glucose graph (bottom block)
};

CellType cellTypeFromString(const String& s);

// What the blocks show.
struct CellView {
  Reading last;
  Deltas deltas;
  bool mmol = false;
  int low = 70, high = 180;  // limits in mg/dL, for the statistics
};

struct Cell {
  CellType type = CELL_NONE;
  lv_obj_t* parent = nullptr;
  int x = 0, y = 0, w = 0, h = 0;  // position in `parent`
  bool middleRow = false;          // small content is centred on the value block there, not on the cell
  // widgets and state, owned by cells.cpp
  lv_obj_t *a = nullptr, *b = nullptr, *c = nullptr;
  lv_obj_t* seg[5] = {};
  Trend trend = TREND_UNKNOWN;
  bool initDone = false;
  long stamp = 0;
};

// Creates the block's widgets (the previous ones must be gone: the screen is rebuilt as a whole).
void cellBuild(Cell& cell);

// Sets the block's content. A widget is only touched when its content changed, so calling this often
// (every second) costs no panel refresh.
void cellUpdate(Cell& cell, const CellView& view);
