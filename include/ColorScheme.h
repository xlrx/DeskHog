#ifndef COLOR_SCHEME_H
#define COLOR_SCHEME_H

#include "lvgl.h"

class ColorScheme {
public:
    // Text colors
    static lv_color_t labelColor() {
        return lv_color_hex(0x333333);  // Dark gray for labels
    }

    static lv_color_t valueColor() {
        return lv_color_hex(0x000000);  // Black for values
    }

    // Background colors
    static lv_color_t backgroundColor() {
        return lv_color_hex(0xFFFFFF);  // White background
    }

private:
    // Private constructor to prevent instantiation
    ColorScheme() {}
};

#endif // COLOR_SCHEME_H 