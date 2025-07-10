#pragma once

#include <lvgl.h>
#include "ui/InputHandler.h"
#include "../flappy_bird.h"

/**
 * @class FlappyHogCard
 * @brief Card wrapper for the Flappy Hog game
 * 
 * Provides a card interface for the Flappy Bird-style game,
 * handling input and managing the game lifecycle within the card system.
 */
class FlappyHogCard : public InputHandler {
public:
    /**
     * @brief Constructor
     * @param parent Parent LVGL object
     */
    FlappyHogCard(lv_obj_t* parent);
    
    /**
     * @brief Destructor - cleans up game resources
     */
    ~FlappyHogCard();
    
    /**
     * @brief Get the card's LVGL object
     * @return LVGL object representing this card
     */
    lv_obj_t* getCard();
    
    /**
     * @brief Handle button press events
     * @param button_index Index of button pressed
     * @return true if button was handled
     */
    bool handleButtonPress(uint8_t button_index) override;
    
    /**
     * @brief Prepare card for removal from stack
     * 
     * Called before the card's LVGL object is deleted.
     * Prevents double-deletion of LVGL objects.
     */
    void prepareForRemoval() override;
    
    /**
     * @brief Main game loop - should be called regularly when card is active
     */
    void loop();
    
    /**
     * @brief Check if this card is currently active
     * @return true if the card is the currently visible card
     */
    bool isActive() const;

private:
    FlappyBirdGame* game;           ///< The actual game instance
    lv_obj_t* cardContainer;        ///< LVGL container for the game
    bool markedForRemoval;          ///< Flag to prevent double deletion
};