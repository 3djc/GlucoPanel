#pragma once

// Glue between LVGL and the ePaper panel (Seeed_GFX's EPaper on the device, an SDL window in the sim).
//
// LVGL renders the widget tree into a small RGB565 buffer; the flush callback turns each pixel into
// black or white and writes it to the panel's framebuffer. The panel itself is only refreshed from
// portRefresh(), which decides between a full and a partial refresh.

// Starts the panel and LVGL. Call once, first thing in setup().
void portBegin();

// Renders whatever changed in the widget tree and refreshes the panel.
//   forceFull = true : redraw the whole screen and do a full refresh (screen layout changed).
//   forceFull = false: partial refresh of the smallest window covering the changes, upgraded to a
//                      full refresh every FULL_REFRESH_MS to clear ghosting. Nothing is refreshed
//                      when nothing changed.
void portRefresh(bool forceFull);
