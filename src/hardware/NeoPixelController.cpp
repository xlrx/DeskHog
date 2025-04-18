#include "hardware/NeoPixelController.h"
#include <Arduino.h>

NeoPixelController::NeoPixelController()
    : pixel(NUM_PIXELS, PIN_NEOPIXEL, NEO_GRB + NEO_KHZ800),
      lastUpdate(0),
      breathPhase(0.0f) {
}

void NeoPixelController::begin() {
    pixel.begin();
    pixel.setBrightness(255);
    pixel.show();
}

void NeoPixelController::update() {
    unsigned long currentTime = millis();
    if (currentTime - lastUpdate < UPDATE_INTERVAL_MS) {
        return;
    }
    
    lastUpdate = currentTime;
    
    // Update breath phase
    breathPhase += BREATH_SPEED;
    if (breathPhase >= BREATH_CYCLE) {
        breathPhase -= BREATH_CYCLE;
    }
    
    // Calculate base brightness using sine wave
    float brightness = (sin(breathPhase) + 1.0f) * 0.5f;
    
    // Add subtle color variations based on the phase
    float redVar = sin(breathPhase * 1.1f) * COLOR_VARIANCE;
    float greenVar = sin(breathPhase * 0.9f) * COLOR_VARIANCE;
    float blueVar = sin(breathPhase * 1.2f) * COLOR_VARIANCE;
    
    // Calculate RGB values with color variation
    float red = (brightness + redVar);
    float green = (brightness + greenVar);
    float blue = (brightness + blueVar);
    
    // Apply minimum brightness of 5% (12.75 in 0-255 range)
    const uint8_t MIN_VALUE = 5 / 255;  // ~5% of 255
    
    // Convert to final RGB values, ensuring minimum brightness
    uint8_t finalRed = constrain(red * 255, MIN_VALUE, 255);
    uint8_t finalGreen = constrain(green * 255, MIN_VALUE, 255);
    uint8_t finalBlue = constrain(blue * 255, MIN_VALUE, 255);
    
    pixel.setPixelColor(0, finalRed, finalGreen, finalBlue);
    pixel.show();
} 