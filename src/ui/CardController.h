#pragma once

#include <Arduino.h>
#include <lvgl.h>
#include <vector>

#include "ConfigManager.h"
#include "hardware/WifiInterface.h"
#include "posthog/PostHogClient.h"
#include "ui/CardNavigationStack.h"
#include "ui/ProvisioningCard.h"
#include "ui/InsightCard.h"
#include "ui/FriendCard.h"
#include "hardware/DisplayInterface.h"
#include "EventQueue.h"
#include "config/CardConfig.h"

/**
 * @class CardController
 * @brief Main UI controller managing card-based interface components
 * 
 * Coordinates the creation, management, and interaction between different
 * UI cards including insights, provisioning, and animations. Handles thread-safe
 * display updates and event-driven UI state changes.
 */
class CardController {
public:
    /**
     * @brief Constructor for CardController
     * 
     * @param screen LVGL screen object
     * @param screenWidth Width of the display in pixels
     * @param screenHeight Height of the display in pixels
     * @param configManager Reference to configuration manager
     * @param wifiInterface Reference to WiFi interface
     * @param posthogClient Reference to PostHog client
     * @param eventQueue Reference to event queue for state changes
     */
    CardController(
        lv_obj_t* screen,
        uint16_t screenWidth,
        uint16_t screenHeight,
        ConfigManager& configManager,
        WiFiInterface& wifiInterface,
        PostHogClient& posthogClient,
        EventQueue& eventQueue
    );
    
    /**
     * @brief Destructor - cleans up UI resources
     */
    ~CardController();
    
    /**
     * @brief Initialize the controller with a display interface
     * @param display Pointer to display interface for thread-safe updates
     */
    void initialize(DisplayInterface* display);

    /**
     * @brief Set the display interface for thread-safe updates
     * @param display Pointer to display interface
     */
    void setDisplayInterface(DisplayInterface* display);
    
    /**
     * @brief Create and add a new insight card to the UI
     * @param insightId Unique identifier for the insight
     */
    void createInsightCard(const String& insightId);
    
    /**
     * @brief Get the card navigation stack
     * @return Pointer to card navigation stack
     */
    CardNavigationStack* getCardStack() { return cardStack; }

    /**
     * @brief Get the provisioning card
     * @return Pointer to provisioning card
     */
    ProvisioningCard* getProvisioningCard() { return provisioningCard; }

    /**
     * @brief Get the animation card
     * @return Pointer to animation card
     */
    FriendCard* getAnimationCard() { return animationCard; }

    /**
     * @brief Get all insight cards
     * @return Reference to vector of insight card pointers
     */
    std::vector<InsightCard*>& getInsightCards() { return insightCards; }

    /**
     * @brief Get the display interface
     * @return Pointer to display interface
     */
    DisplayInterface* getDisplayInterface() { return displayInterface; }

    /**
     * @brief Get available card definitions for the web UI
     * @return Vector of CardDefinition objects representing available card types
     */
    std::vector<CardDefinition> getCardDefinitions() const;

    /**
     * @brief Register an available card type with its definition and factory function
     * @param definition The card definition including metadata and factory function
     */
    void registerCardType(const CardDefinition& definition);

    /**
     * @brief Process card configuration changes from the web UI
     * Called when CARD_CONFIG_CHANGED event is received
     */
    void handleCardConfigChanged();

private:
    // Screen reference
    lv_obj_t* screen;              ///< Main LVGL screen object
    uint16_t screenWidth;          ///< Screen width in pixels
    uint16_t screenHeight;         ///< Screen height in pixels
    
    // System components
    ConfigManager& configManager;   ///< Configuration manager reference
    WiFiInterface& wifiInterface;  ///< WiFi interface reference
    PostHogClient& posthogClient;  ///< PostHog client reference
    EventQueue& eventQueue;        ///< Event queue reference
    
    // UI Components
    CardNavigationStack* cardStack;     ///< Navigation stack for cards
    ProvisioningCard* provisioningCard; ///< Card for device provisioning
    FriendCard* animationCard;       ///< Card for animations
    std::vector<InsightCard*> insightCards; ///< Collection of insight cards
    
    // Display interface for thread safety
    DisplayInterface* displayInterface;  ///< Thread-safe display interface
    
    // Card registration and management
    std::vector<CardDefinition> registeredCardTypes; ///< Available card types with factory functions
    std::vector<CardConfig> currentCardConfigs;      ///< Current card configuration from storage
    
    /**
     * @brief Create and initialize the animation card
     */
    void createAnimationCard();
    
    /**
     * @brief Handle insight-related events
     * @param event Event containing insight data
     */
    void handleInsightEvent(const Event& event);

    /**
     * @brief Handle WiFi-related events
     * @param event Event containing WiFi state
     */
    void handleWiFiEvent(const Event& event);

    /**
     * @brief Initialize default card type registrations
     * Registers built-in card types (INSIGHT, FRIEND) with their factory functions
     */
    void initializeCardTypes();

    /**
     * @brief Reconcile current cards with new configuration
     * Diffs configuration, removes old cards, creates new ones, and reorders
     * @param newConfigs New card configuration from storage
     */
    void reconcileCards(const std::vector<CardConfig>& newConfigs);
}; 