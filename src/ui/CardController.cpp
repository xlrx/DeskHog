#include "ui/CardController.h"

CardController::CardController(
    lv_obj_t* screen,
    uint16_t screenWidth,
    uint16_t screenHeight,
    ConfigManager& configManager,
    WiFiInterface& wifiInterface,
    PostHogClient& posthogClient
) : screen(screen),
    screenWidth(screenWidth),
    screenHeight(screenHeight),
    configManager(configManager),
    wifiInterface(wifiInterface),
    posthogClient(posthogClient),
    cardStack(nullptr),
    provisioningCard(nullptr)
{
}

CardController::~CardController() {
    // Clean up any allocated resources
    delete cardStack;
    delete provisioningCard;
    
    for (auto* card : insightCards) {
        delete card;
    }
    insightCards.clear();
}

void CardController::initialize() {
    // Create card navigation stack
    cardStack = new CardNavigationStack(screen, screenWidth, screenHeight);
    
    // Create provision UI
    provisioningCard = new ProvisioningCard(
        screen, 
        wifiInterface, 
        screenWidth, 
        screenHeight
    );
    
    // Add provisioning card to navigation stack
    cardStack->addCard(provisioningCard->getCard());
    
    // Get count of insights to determine card count
    std::vector<String> insightIds = configManager.getAllInsightIds();
    
    // Create and add insight cards
    for (const String& id : insightIds) {
        auto* insightCard = new InsightCard(
            screen,
            configManager,
            posthogClient,
            id,
            screenWidth,
            screenHeight
        );
        cardStack->addCard(insightCard->getCard());
        insightCards.push_back(insightCard);
    }
    
    // Connect WiFi manager to UI
    wifiInterface.setUI(provisioningCard);
}

void CardController::setMutex(SemaphoreHandle_t* mutex) {
    if (cardStack) {
        cardStack->setMutex(mutex);
    }
} 