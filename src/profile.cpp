#include "profile.h"
#include "ui.h"

#ifdef UI_PROFILE_LCD

LV_FONT_DECLARE(font_lcd_12);
LV_FONT_DECLARE(font_lcd_13);
LV_FONT_DECLARE(font_lcd_18);
LV_FONT_DECLARE(font_lcd_24);
LV_FONT_DECLARE(font_lcd_34);
LV_FONT_DECLARE(font_lcd_big_84);
LV_FONT_DECLARE(font_lcd_big_64);

// JC3248W535: 3.5" IPS, 480x320 (landscape), colour.
const Metrics M = {
    480, 320,
    4, 6, 472, 87, 7, 9,     // band
    96, 107,                  // middle row
    215, 90,                  // graph
    306, 6,                   // error strip
    100,                      // tallH
    80, 60,                   // arrow box, length
    82, 62, 64, 3, 2, 3,      // freshness bar
    40, 10, 7, 18,            // graph margins
    3, 2, 5, 4, 7, 22, 5, 3,  // graph strokes and labels
    &font_lcd_12, &font_lcd_13, &font_lcd_18, &font_lcd_24, &font_lcd_34, &font_lcd_big_84, &font_lcd_big_64,
    &font_lcd_34,
    190,
};

#else

// reTerminal E1001: 7.5" e-paper, 800x480, black and white.
const Metrics M = {
    800, 480,
    0, 0, 800, 140, 0, 14,    // band: the top row
    140, 200,                 // middle row
    340, 140,                 // graph: the rest of the screen
    458, 10,                  // error strip (drawn over the bottom of the graph)
    150,                      // tallH
    130, 112,                 // arrow box, length
    140, 100, 98, 5, 3, 5,    // freshness bar
    56, 14, 10, 26,           // graph margins
    4, 3, 7, 6, 9, 30, 7, 4,  // graph strokes and labels
    &font_ui_16, &font_ui_16, &font_ui_24, &font_ui_32, &font_ui_48, &font_big_128, &font_big_96,
    &font_ui_72,
    280,
};

#endif
