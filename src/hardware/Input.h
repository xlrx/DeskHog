#pragma once

#include <Arduino.h>
#include <Bounce2.h>

// Forward declare the external buttons array
extern Bounce2::Button buttons[];

class Input {
public:
    static const uint8_t BUTTON_DOWN = 0;    // BOOT button - pulled HIGH by default, LOW when pressed
    static const uint8_t BUTTON_CENTER = 1;  //pulled LOW by default, HIGH when pressed
    static const uint8_t BUTTON_UP = 2;      //pulled LOW by default, HIGH when pressed

    static void configureButtons() {
        // BOOT button has built-in pullup, active LOW
        buttons[BUTTON_DOWN].attach(BUTTON_DOWN, INPUT_PULLUP);
        buttons[BUTTON_DOWN].interval(5);
        buttons[BUTTON_DOWN].setPressedState(LOW);
        
        // Configure CENTER and UP buttons with pulldown, active HIGH
        buttons[BUTTON_CENTER].attach(BUTTON_CENTER, INPUT_PULLDOWN);
        buttons[BUTTON_CENTER].interval(5);
        buttons[BUTTON_CENTER].setPressedState(HIGH);
        
        buttons[BUTTON_UP].attach(BUTTON_UP, INPUT_PULLDOWN);
        buttons[BUTTON_UP].interval(5);
        buttons[BUTTON_UP].setPressedState(HIGH);
    }

    static void update() {
        buttons[BUTTON_DOWN].update();
        buttons[BUTTON_CENTER].update();
        buttons[BUTTON_UP].update();
    }

    static bool isDownPressed() { return buttons[BUTTON_DOWN].pressed(); }
    static bool isCenterPressed() { return buttons[BUTTON_CENTER].pressed(); }
    static bool isUpPressed() { return buttons[BUTTON_UP].pressed(); }

    static bool isDownReleased() { return buttons[BUTTON_DOWN].released(); }
    static bool isCenterReleased() { return buttons[BUTTON_CENTER].released(); }
    static bool isUpReleased() { return buttons[BUTTON_UP].released(); }
};

