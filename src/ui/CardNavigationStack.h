#ifndef CARD_NAVIGATION_STACK_H
#define CARD_NAVIGATION_STACK_H

#include <Arduino.h>
#include <lvgl.h>
#include <Bounce2.h>

// Forward declaration
class DisplayInterface;

class CardNavigationStack {
public:
    // Constructor - removed num_cards parameter
    CardNavigationStack(lv_obj_t* parent, uint16_t width, uint16_t height);
    
    // Add an existing LVGL object as a card
    void addCard(lv_obj_t* card);
    
    // Remove an existing card
    bool removeCard(lv_obj_t* card);
    
    // Legacy method for simple colored cards with labels (deprecated)
    void addCard(lv_color_t color, const char* label_text);
    
    // Navigate to next card
    void nextCard();
    
    // Navigate to previous card
    void prevCard();
    
    // Navigate to a specific card by index
    void goToCard(uint8_t index);
    
    // Get current card index
    uint8_t getCurrentIndex() const;
    
    // Process button input
    void handleButtonPress(uint8_t button_index);
    
    // Button handling task
    static void buttonTask(void* parameter);
    
    // Set mutex reference
    void setMutex(SemaphoreHandle_t* mutex_ptr);
    
private:
    // LVGL objects
    lv_obj_t* _parent;
    lv_obj_t* _main_container;
    lv_obj_t* _scroll_indicator;
    
    // State
    uint8_t _current_card;
    uint16_t _width;
    uint16_t _height;
    
    // Mutex reference
    SemaphoreHandle_t* _mutex_ptr;
    
    // Static instance pointer for task
    static CardNavigationStack* _instance;
    
    // Private methods
    static void _scroll_event_cb(lv_event_t* e);
    void _update_scroll_indicator(int active_index);
    void _update_pip_count();
};

#endif // CARD_NAVIGATION_STACK_H 