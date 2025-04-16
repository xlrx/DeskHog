#pragma once

#include <vector>
#include "ui/CardNavigationStack.h"
#include "ui/ProvisioningCard.h"
#include "ui/InsightCard.h"
#include "ConfigManager.h"
#include "hardware/WifiInterface.h"
#include "posthog/PostHogClient.h"

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

public:
    CardController(
        lv_obj_t* screen, 
        uint16_t screenWidth, 
        uint16_t screenHeight,
        ConfigManager& configManager,
        WiFiInterface& wifiInterface,
        PostHogClient& posthogClient
    );
    
    ~CardController();
    
    void initialize();
    void setMutex(SemaphoreHandle_t* mutex);
    
    CardNavigationStack* getCardStack() { return cardStack; }
    ProvisioningCard* getProvisioningCard() { return provisioningCard; }
    std::vector<InsightCard*>& getInsightCards() { return insightCards; }
}; 