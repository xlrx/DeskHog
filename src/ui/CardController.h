#pragma once

#include <vector>
#include "ui/CardNavigationStack.h"
#include "ui/ProvisioningCard.h"
#include "ui/InsightCard.h"
#include "ConfigManager.h"
#include "hardware/WifiInterface.h"
#include "hardware/DisplayInterface.h"
#include "posthog/PostHogClient.h"
#include "EventQueue.h"

class CardController {
private:
    CardNavigationStack* cardStack;
    ProvisioningCard* provisioningCard;
    std::vector<InsightCard*> insightCards;
    
    lv_obj_t* screen;
    uint16_t screenWidth;
    uint16_t screenHeight;
    ConfigManager& configManager;
    WiFiInterface& wifiInterface;
    PostHogClient& posthogClient;
    DisplayInterface* displayInterface;
    EventQueue& eventQueue;

    // Handle insight events internally
    void handleInsightEvent(const Event& event);
    
    // Handle WiFi events internally
    void handleWiFiEvent(const Event& event);

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
    
    // Set the display interface for thread-safe operations
    void setDisplayInterface(DisplayInterface* display);
    
    // Create an insight card and add it to the UI
    InsightCard* createInsightCard(const String& insightId);
    
    CardNavigationStack* getCardStack() { return cardStack; }
    ProvisioningCard* getProvisioningCard() { return provisioningCard; }
    std::vector<InsightCard*>& getInsightCards() { return insightCards; }
    DisplayInterface* getDisplayInterface() { return displayInterface; }
}; 