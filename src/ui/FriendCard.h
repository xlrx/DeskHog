#pragma once

#include <lvgl.h>
#include <Arduino.h>
#include <memory>
#include <functional>
#include <vector>
#include <string>
#include "ui/InputHandler.h"

/**
 * @class AnimationCard
 * @brief UI component for displaying animated sprites
 * 
 * Uses LVGL's built-in animation image widget (lv_animimg)
 */
class AnimationCard : public InputHandler {
public:
    /**
     * @brief Constructor
     * 
     * @param parent LVGL parent object
     * @param width Card width
     * @param height Card height
     */
    AnimationCard(lv_obj_t* parent);
    
    /**
     * @brief Destructor - safely cleans up UI resources
     */
    ~AnimationCard();
    
    /**
     * @brief Get the underlying LVGL card object
     * 
     * @return LVGL object pointer
     */
    lv_obj_t* getCard();
    
    /**
     * @brief Start the sprite animation
     */
    void startAnimation();
    
    /**
     * @brief Stop the sprite animation
     */
    void stopAnimation();
    
    /**
     * @brief Set text for both the main label and shadow label
     * 
     * @param text The text to display
     */
    void setText(const char* text);
    
    /**
     * @brief Add a message to the cycling messages list
     * 
     * @param message The message to add
     */
    void addMessage(const char* message);
    
    /**
     * @brief Cycle to the next message in the list
     */
    void cycleNextMessage();
    
    /**
     * @brief Handle button press events
     * 
     * @param button_index The index of the button that was pressed
     * @return true if handled, false otherwise
     */
    bool handleButtonPress(uint8_t button_index) override;
    
private:
    // Constants
    static constexpr int ANIMATION_DURATION_MS = 1000; // Total animation duration in ms
    
    /**
     * @brief A safe wrapper to check if an LVGL object is valid
     * 
     * @param obj LVGL object to check
     * @return true if valid, false otherwise
     */
    bool isValidObject(lv_obj_t* obj) const;
    
    /**
     * @brief Create a text label with consistent styling
     * 
     * @param parent Parent LVGL object
     * @param color Text color
     * @param x_offset X offset for alignment
     * @param y_offset Y offset for alignment
     * @return lv_obj_t* The created label object
     */
    lv_obj_t* createStyledLabel(lv_obj_t* parent, lv_color_t color, int16_t x_offset, int16_t y_offset);
    
    // UI Elements
    lv_obj_t* _card;
    lv_obj_t* _background;
    lv_obj_t* _anim_img;
    lv_obj_t* _label;
    lv_obj_t* _label_shadow;
    
    // Animation state
    bool _animation_running;
    
    // Message cycling
    std::vector<std::string> _messages;
    size_t _current_message_index;
}; 