#include "ui/CardController.h"

CardController::CardController(
    lv_obj_t* screen,
    uint16_t screenWidth,
    uint16_t screenHeight,
    ConfigManager& configManager,
    WiFiInterface& wifiInterface,
    PostHogClient& posthogClient,
    EventQueue& eventQueue
) : screen(screen),
    screenWidth(screenWidth),
    screenHeight(screenHeight),
    configManager(configManager),
    wifiInterface(wifiInterface),
    posthogClient(posthogClient),
    eventQueue(eventQueue),
    cardStack(nullptr),
    provisioningCard(nullptr),
    displayInterface(nullptr)
{
}

CardController::~CardController() {
    // Clean up any allocated resources
    delete cardStack;
    cardStack = nullptr;
    
    delete provisioningCard;
    provisioningCard = nullptr;
    
    // Use mutex if available before cleaning up insight cards
    if (displayInterface && displayInterface->getMutexPtr()) {
        xSemaphoreTake(*(displayInterface->getMutexPtr()), portMAX_DELAY);
    }
    
    // Clean up insight cards
    for (auto* card : insightCards) {
        delete card;
    }
    insightCards.clear();
    
    // Release mutex if we took it
    if (displayInterface && displayInterface->getMutexPtr()) {
        xSemaphoreGive(*(displayInterface->getMutexPtr()));
    }
}

void CardController::initialize(DisplayInterface* display) {
    // Set the display interface first
    setDisplayInterface(display);
    
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
        createInsightCard(id);
    }
    
    // Connect WiFi manager to UI
    wifiInterface.setUI(provisioningCard);
    
    // Subscribe to insight events
    eventQueue.subscribe([this](const Event& event) {
        handleInsightEvent(event);
    });
}

void CardController::setDisplayInterface(DisplayInterface* display) {
    displayInterface = display;
    
    // Set the mutex for the card stack if we have a display interface
    if (cardStack && displayInterface) {
        cardStack->setMutex(displayInterface->getMutexPtr());
    }
}

// Create an insight card and add it to the UI
InsightCard* CardController::createInsightCard(const String& insightId) {
    if (!displayInterface || !displayInterface->takeMutex(portMAX_DELAY)) {
        return nullptr;
    }
    
    // Create new insight card using full screen dimensions
    InsightCard* insightCard = new InsightCard(
        screen,
        configManager,
        posthogClient,
        insightId,
        screenWidth,
        screenHeight
    );
    
    // Add to navigation stack
    cardStack->addCard(insightCard->getCard());
    
    // Add to our list of cards
    insightCards.push_back(insightCard);
    
    displayInterface->giveMutex();
    return insightCard;
}

// Handle insight events
void CardController::handleInsightEvent(const Event& event) {
    if (event.type == EventType::INSIGHT_ADDED) {
        createInsightCard(event.insightId);
    } 
    else if (event.type == EventType::INSIGHT_DELETED) {
        if (!displayInterface || !displayInterface->takeMutex(portMAX_DELAY)) {
            return;
        }
        
        // Find and remove the card
        for (auto it = insightCards.begin(); it != insightCards.end(); ++it) {
            InsightCard* card = *it;
            if (card->getInsightId() == event.insightId) {
                // Remove from card stack
                cardStack->removeCard(card->getCard());
                
                // Remove from vector and delete
                insightCards.erase(it);
                delete card;
                break;
            }
        }
        
        displayInterface->giveMutex();
    }
} 