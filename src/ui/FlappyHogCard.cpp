#include "ui/FlappyHogCard.h"
#include "hardware/Input.h"

// Static timer for game updates
static lv_timer_t* gameTimer = nullptr;
static FlappyHogCard* activeCard = nullptr;

// Timer callback function
static void game_timer_cb(lv_timer_t* timer) {
    FlappyHogCard* card = static_cast<FlappyHogCard*>(timer->user_data);
    if (card) {
        card->loop();
    }
}

FlappyHogCard::FlappyHogCard(lv_obj_t* parent) 
    : game(nullptr)
    , cardContainer(nullptr)
    , markedForRemoval(false) {
    
    // Create the game instance
    game = new FlappyBirdGame();
    
    // Setup the game with the parent screen
    game->setup(parent);
    
    // Get the game's container
    cardContainer = game->get_main_container();
    
    // Create a timer to call the game loop regularly (60 FPS = ~16ms)
    gameTimer = lv_timer_create(game_timer_cb, 16, this);
    activeCard = this;
}

FlappyHogCard::~FlappyHogCard() {
    // Clean up the timer
    if (gameTimer && activeCard == this) {
        lv_timer_del(gameTimer);
        gameTimer = nullptr;
        activeCard = nullptr;
    }
    
    // Clean up game resources
    if (game && !markedForRemoval) {
        game->cleanup();
        delete game;
        game = nullptr;
    }
    
    // The LVGL object will be deleted by CardNavigationStack
    // if prepareForRemoval() was called
    cardContainer = nullptr;
}

lv_obj_t* FlappyHogCard::getCard() {
    return cardContainer;
}

bool FlappyHogCard::handleButtonPress(uint8_t button_index) {
    // The game handles its own center button internally
    // We don't need to handle navigation buttons here
    // as they're handled by CardNavigationStack
    
    // Return false to allow normal navigation
    return false;
}

void FlappyHogCard::prepareForRemoval() {
    // Mark that we're being removed to prevent double deletion
    markedForRemoval = true;
    
    // The CardNavigationStack will delete the LVGL object
    // We just need to ensure we don't try to delete it again
}

void FlappyHogCard::loop() {
    if (game) {
        game->loop();
    }
}

bool FlappyHogCard::isActive() const {
    // Card is active if it exists and has a valid container
    return game != nullptr && cardContainer != nullptr;
}