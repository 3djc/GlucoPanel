#pragma once
#include <lvgl.h>
#include <time.h>

// Glucose graph for the dashboard: the last GRAPH_HOURS of readings as a line, the low..high target
// range as a light-grey band with dashed limits, hour ticks along the bottom. The y range follows the
// data. Custom-drawn, so it costs one widget and no extra memory per point.
lv_obj_t* graphCreate(lv_obj_t* parent, int x, int y, int w, int h);

// Reloads the readings collected by the fetchers and the low/high thresholds from the settings, and
// redraws. The time axis ends at `end` (the newest reading).
void graphUpdate(lv_obj_t* graph, time_t end, bool mmol);
