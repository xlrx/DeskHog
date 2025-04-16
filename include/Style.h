#ifndef COLOR_SCHEME_H
#define COLOR_SCHEME_H

#include "lvgl.h"
#include "fonts/Inter_18pt_Regular.h"
#include "fonts/Inter_18pt_SemiBold.h"

class Style {
private:
    static const lv_font_t* _label_font;
    static const lv_font_t* _value_font;
    static const lv_font_t* _large_value_font;
    static bool _fonts_initialized;

    // Initialize fonts - implemented in Style.cpp
    static void initFonts();

public:
    // Initialize all style resources
    static void init() {
        initFonts();
    }

    // Font getters
    static const lv_font_t* labelFont() {
        initFonts();
        return _label_font ? _label_font : &lv_font_montserrat_14;
    }

    static const lv_font_t* valueFont() {
        initFonts();
        return _value_font ? _value_font : &lv_font_montserrat_18;
    }

    static const lv_font_t* largeValueFont() {
        initFonts();
        return _large_value_font ? _large_value_font : &lv_font_montserrat_36;
    }

    // Text colors
    static lv_color_t labelColor() {
        return lv_color_hex(0xAAAAAA);  // Dark gray for labels
    }

    static lv_color_t valueColor() {
        return lv_color_hex(0xFFFFFF);
    }

    // Background colors
    static lv_color_t backgroundColor() {
        return lv_color_hex(0x000000);
    }

private:
    // Private constructor to prevent instantiation
    Style() {}
};

// Static member declarations are in the header
// Definitions will be in Style.cpp

#endif // COLOR_SCHEME_H 