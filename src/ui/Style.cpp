#include "Style.h"
#include "esp_heap_caps.h"
#include <Arduino.h>
#include "fonts/Inter_18pt_Regular.h"
#include "fonts/Inter_18pt_SemiBold.h"

// Initialize static members
const lv_font_t* Style::_label_font = nullptr;
const lv_font_t* Style::_value_font = nullptr;
const lv_font_t* Style::_large_value_font = nullptr;
bool Style::_fonts_initialized = false;

void Style::initFonts() {
    if (_fonts_initialized) return;

    Serial.println("Initializing custom fonts...");
    Serial.printf("Free PSRAM: %d bytes\n", ESP.getFreePsram());
    Serial.printf("Free heap: %d bytes\n", ESP.getFreeHeap());
    
    // We'll attempt to create these incrementally if possible
    bool success = true;
    
#if LV_USE_TINY_TTF
    // Start with smaller fonts first to increase chance of success
    _label_font = lv_tiny_ttf_create_data(Inter_18pt_Regular_data, Inter_18pt_Regular_size, 15);
    if (_label_font) {
        Serial.println("Label font created successfully (12pt)");
    } else {
        Serial.println("Failed to create label font - falling back to built-in");
        success = false;
    }
    
    if (success) {
        _value_font = lv_tiny_ttf_create_data(Inter_18pt_SemiBold_data, Inter_18pt_SemiBold_size, 16);
        if (_value_font) {
            Serial.println("Value font created successfully (10pt)");
        } else {
            Serial.println("Failed to create value font - falling back to built-in");
            success = false;
        }
    }
    
    if (success) {
        _large_value_font = lv_tiny_ttf_create_data(Inter_18pt_SemiBold_data, Inter_18pt_SemiBold_size, 36);
        if (_large_value_font) {
            Serial.println("Large value font created successfully (28pt)");
        } else {
            Serial.println("Failed to create large value font - falling back to built-in");
            success = false;
        }
    }
    
    if (_label_font && _value_font && _large_value_font) {
        _fonts_initialized = true;
        Serial.println("All fonts initialized successfully");
    } else {
        Serial.println("Font initialization incomplete - using built-in fonts");
        // Fall back to built-in fonts for any that failed
        if (!_label_font) _label_font = &lv_font_montserrat_14;
        if (!_value_font) _value_font = &lv_font_montserrat_18;
        if (!_large_value_font) _large_value_font = &lv_font_montserrat_36;
        _fonts_initialized = true;
    }
#else
    Serial.println("TinyTTF not enabled, using default fonts");
    // Use built-in fonts
    _label_font = &lv_font_montserrat_14;
    _value_font = &lv_font_montserrat_18;
    _large_value_font = &lv_font_montserrat_36;
    _fonts_initialized = true;
#endif

    Serial.printf("After font init - Free PSRAM: %d bytes\n", ESP.getFreePsram());
    Serial.printf("After font init - Free heap: %d bytes\n", ESP.getFreeHeap());
} 