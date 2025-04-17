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

class CardController {
public:
    CardController(
        lv_obj_t* screen,
        uint16_t screenWidth,
        uint16_t screenHeight,
        ConfigManager& configManager,
        WiFiInterface& wifiInterface,
        PostHogClient& posthogClient,
        EventQueue& eventQueue
    );
    
    ~CardController();
    
    void initialize(DisplayInterface* display);
    void setDisplayInterface(DisplayInterface* display);
    
    // Create an insight card and add it to the UI
    InsightCard* createInsightCard(const String& insightId);
    
    // Getters for UI components
    CardNavigationStack* getCardStack() { return cardStack; }
    ProvisioningCard* getProvisioningCard() { return provisioningCard; }
    AnimationCard* getAnimationCard() { return animationCard; }
    std::vector<InsightCard*>& getInsightCards() { return insightCards; }
    DisplayInterface* getDisplayInterface() { return displayInterface; }

private:
    // Screen reference
    lv_obj_t* screen;
    uint16_t screenWidth;
    uint16_t screenHeight;
    
    // System components
    ConfigManager& configManager;
    WiFiInterface& wifiInterface;
    PostHogClient& posthogClient;
    EventQueue& eventQueue;
    
    // UI Components
    CardNavigationStack* cardStack;
    ProvisioningCard* provisioningCard;
    AnimationCard* animationCard;
    std::vector<InsightCard*> insightCards;
    
    // Display interface for thread safety
    DisplayInterface* displayInterface;
    
    // Private methods
    void createAnimationCard();
    
    // Event handlers
    void handleInsightEvent(const Event& event);
    void handleWiFiEvent(const Event& event);
}; 