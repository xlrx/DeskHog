#pragma once

#include <lvgl.h>
#include <Arduino.h>
#include <Bounce2.h>
#include <vector>
#include "ui/InputHandler.h"

// Forward declaration
class DisplayInterface;

class CardNavigationStack {
public:
    // Constructor - removed num_cards parameter
    CardNavigationStack(lv_obj_t* parent, uint16_t width, uint16_t height);
    
    // Add a card to the navigation stack
    void addCard(lv_obj_t* card);
    
    // Add a simple colored card with text
    void addCard(lv_color_t color, const char* label_text);
    
    // Remove a card from the stack (returns true if successful)
    bool removeCard(lv_obj_t* card);
    
    // Navigation methods
    void nextCard();
    void prevCard();
    void goToCard(uint8_t index);
    
    // Get current card index
    uint8_t getCurrentIndex() const;
    
    // Set mutex for thread-safe button handling
    void setMutex(SemaphoreHandle_t* mutex_ptr);
    
    // Handle button press events
    void handleButtonPress(uint8_t button_index);
    
    // Register a card as an input handler
    void registerInputHandler(lv_obj_t* card, InputHandler* handler);
    
private:
    // LVGL event callback for scrolling
    static void _scroll_event_cb(lv_event_t* e);
    
    // Update the scroll indicator pips
    void _update_pip_count();
    void _update_scroll_indicator(int active_index);
    
    // UI elements
    lv_obj_t* _parent;
    lv_obj_t* _main_container;
    lv_obj_t* _scroll_indicator;
    
    // Dimensions
    uint16_t _width;
    uint16_t _height;
    
    // Navigation state
    uint8_t _current_card;
    
    // Thread safety
    SemaphoreHandle_t* _mutex_ptr;
    
    // Input handlers for each card
    std::vector<std::pair<lv_obj_t*, InputHandler*>> _input_handlers;
}; 