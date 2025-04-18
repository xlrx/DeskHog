#pragma once

#include <Adafruit_NeoPixel.h>

class NeoPixelController {
public:
    NeoPixelController();
    void begin();
    void update();

private:
    static constexpr uint8_t NUM_PIXELS = 1;
    static constexpr uint16_t UPDATE_INTERVAL_MS = 16;  // ~60fps
    static constexpr float BREATH_SPEED = 0.0167f;  // Reduced to 1/3 of original speed
    static constexpr float BREATH_CYCLE = 2.0f * PI;
    static constexpr float COLOR_VARIANCE = 0.2f;

    Adafruit_NeoPixel pixel;
    unsigned long lastUpdate;
    float breathPhase;
}; 