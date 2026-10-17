// LVGL configuration, shared by the device build and the SDL2 simulator.
// Only what the UI needs is enabled; everything else keeps LVGL's defaults.
#ifndef LV_CONF_H
#define LV_CONF_H

#define LV_COLOR_DEPTH 16

#define LV_USE_STDLIB_MALLOC LV_STDLIB_CLIB
#define LV_USE_STDLIB_STRING LV_STDLIB_BUILTIN
#define LV_USE_STDLIB_SPRINTF LV_STDLIB_BUILTIN

#define LV_USE_OS LV_OS_NONE
#define LV_USE_LOG 0
#define LV_USE_ASSERT_NULL 1
#define LV_USE_ASSERT_MALLOC 1

// The panel is monochrome and refreshed on demand: no theme, no animations, no input devices.
#define LV_USE_THEME_DEFAULT 0
#define LV_USE_THEME_SIMPLE 0
#define LV_USE_THEME_MONO 0

#define LV_FONT_MONTSERRAT_14 1
#define LV_FONT_MONTSERRAT_16 1
#define LV_FONT_MONTSERRAT_24 1
#define LV_FONT_MONTSERRAT_32 1
#define LV_FONT_MONTSERRAT_48 1
#define LV_FONT_DEFAULT &lv_font_montserrat_16

#define LV_USE_QRCODE 1

#endif /* LV_CONF_H */
