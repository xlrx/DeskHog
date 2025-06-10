#include "ui/CardController.h"
#include <algorithm>

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
    animationCard(nullptr),
    displayInterface(nullptr)
{
}

CardController::~CardController() {
    // Clean up any allocated resources
    delete cardStack;
    cardStack = nullptr;
    
    delete provisioningCard;
    provisioningCard = nullptr;
    
    delete animationCard;
    animationCard = nullptr;
    
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
    
    // Initialize card type registrations
    initializeCardTypes();
    
    // Create card navigation stack
    cardStack = new CardNavigationStack(screen, screenWidth, screenHeight);
    
    // Create provision UI (always present, not configurable)
    provisioningCard = new ProvisioningCard(
        screen, 
        wifiInterface, 
        screenWidth, 
        screenHeight
    );
    
    // Add provisioning card to navigation stack
    cardStack->addCard(provisioningCard->getCard());
    
    // Load current card configuration and create cards
    currentCardConfigs = configManager.getCardConfigs();
    
    // If no configuration exists, create default cards for backward compatibility
    if (currentCardConfigs.empty()) {
        // Create default friend card
        createAnimationCard();
        
        // Create insight cards from legacy storage
        std::vector<String> insightIds = configManager.getAllInsightIds();
        for (const String& id : insightIds) {
            createInsightCard(id);
        }
    } else {
        // Create cards based on stored configuration
        reconcileCards(currentCardConfigs);
    }
    
    // Connect WiFi manager to UI
    wifiInterface.setUI(provisioningCard);
    
    // Subscribe to insight events (legacy support)
    eventQueue.subscribe([this](const Event& event) {
        if (event.type == EventType::INSIGHT_ADDED || 
            event.type == EventType::INSIGHT_DELETED) {
            handleInsightEvent(event);
        }
    });
    
    // Subscribe to card configuration changes
    eventQueue.subscribe([this](const Event& event) {
        if (event.type == EventType::CARD_CONFIG_CHANGED) {
            handleCardConfigChanged();
        }
    });
    
    // Subscribe to WiFi events
    eventQueue.subscribe([this](const Event& event) {
        if (event.type == EventType::WIFI_CONNECTING || 
            event.type == EventType::WIFI_CONNECTED ||
            event.type == EventType::WIFI_CONNECTION_FAILED ||
            event.type == EventType::WIFI_AP_STARTED) {
            handleWiFiEvent(event);
        }
    });
}

void CardController::setDisplayInterface(DisplayInterface* display) {
    displayInterface = display;
    
    // Set the mutex for the card stack if we have a display interface
    if (cardStack && displayInterface) {
        cardStack->setMutex(displayInterface->getMutexPtr());
    }
}

// Create an animation card with the walking sprites
void CardController::createAnimationCard() {
    if (!displayInterface || !displayInterface->takeMutex(portMAX_DELAY)) {
        return;
    }
    
    // Create new animation card
    animationCard = new FriendCard(
        screen
    );
    
    // Add to navigation stack
    cardStack->addCard(animationCard->getCard());
    
    // Register the animation card as an input handler
    cardStack->registerInputHandler(animationCard->getCard(), animationCard);
    
    displayInterface->giveMutex();
}

// Create an insight card and add it to the UI
void CardController::createInsightCard(const String& insightId) {
    // Log current task and core
    Serial.printf("[CardCtrl-DEBUG] createInsightCard called from Core: %d, Task: %s\n", 
                  xPortGetCoreID(), 
                  pcTaskGetTaskName(NULL));

    // Dispatch the actual card creation and LVGL work to the LVGL task
    InsightCard::dispatchToLVGLTask([this, insightId]() {
        Serial.printf("[CardCtrl-DEBUG] LVGL Task creating card for insight: %s from Core: %d, Task: %s\n", 
                      insightId.c_str(), xPortGetCoreID(), pcTaskGetTaskName(NULL));

        if (!displayInterface || !displayInterface->takeMutex(portMAX_DELAY)) {
            Serial.println("[CardCtrl-ERROR] Failed to take mutex in LVGL task for card creation.");
            return;
        }

        // Create new insight card using full screen dimensions
        InsightCard* newCard = new InsightCard(
            screen,         // LVGL parent object
            configManager,  // Dependencies
            eventQueue,
            insightId,
            screenWidth,    // Dimensions
            screenHeight
        );

        if (!newCard || !newCard->getCard()) {
            Serial.printf("[CardCtrl-ERROR] Failed to create InsightCard or its LVGL object for ID: %s\n", insightId.c_str());
            displayInterface->giveMutex();
            delete newCard; // Clean up if partially created
            return;
        }

        // Add to navigation stack
        cardStack->addCard(newCard->getCard());

        // Add to our list of cards (ensure this is thread-safe if accessed elsewhere)
        // The mutex taken above should protect this operation too.
        insightCards.push_back(newCard);
        
        Serial.printf("[CardCtrl-DEBUG] InsightCard for ID: %s created and added to stack.\n", insightId.c_str());

        displayInterface->giveMutex();

        // Request immediate data for this insight now that it's set up
        posthogClient.requestInsightData(insightId);
    });
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

// Handle WiFi events
void CardController::handleWiFiEvent(const Event& event) {
    if (!displayInterface || !displayInterface->takeMutex(portMAX_DELAY)) {
        return;
    }
    
    switch (event.type) {
        case EventType::WIFI_CONNECTING:
            provisioningCard->updateConnectionStatus("Connecting to WiFi...");
            break;
            
        case EventType::WIFI_CONNECTED:
            provisioningCard->updateConnectionStatus("Connected");
            provisioningCard->showWiFiStatus();
            break;
            
        case EventType::WIFI_CONNECTION_FAILED:
            provisioningCard->updateConnectionStatus("Connection failed");
            break;
            
        case EventType::WIFI_AP_STARTED:
            provisioningCard->showQRCode();
            break;
            
        default:
            break;
    }
    
    displayInterface->giveMutex();
}

std::vector<CardDefinition> CardController::getCardDefinitions() const {
    return registeredCardTypes;
}

void CardController::registerCardType(const CardDefinition& definition) {
    registeredCardTypes.push_back(definition);
}

void CardController::initializeCardTypes() {
    // Register INSIGHT card type
    CardDefinition insightDef;
    insightDef.type = CardType::INSIGHT;
    insightDef.name = "PostHog Insight";
    insightDef.allowMultiple = true;
    insightDef.needsConfigInput = true;
    insightDef.configInputLabel = "Insight ID";
    insightDef.uiDescription = "Insight cards let you keep an eye on PostHog data";
    insightDef.factory = [this](const String& configValue) -> lv_obj_t* {
        // Create new insight card using the insight ID
        InsightCard* newCard = new InsightCard(
            screen,
            configManager,
            eventQueue,
            configValue,
            screenWidth,
            screenHeight
        );
        
        if (newCard && newCard->getCard()) {
            // Add to our list of cards
            insightCards.push_back(newCard);
            return newCard->getCard();
        }
        
        delete newCard;
        return nullptr;
    };
    registerCardType(insightDef);
    
    // Register FRIEND card type  
    CardDefinition friendDef;
    friendDef.type = CardType::FRIEND;
    friendDef.name = "Walking Animation";
    friendDef.allowMultiple = false;
    friendDef.needsConfigInput = false;
    friendDef.configInputLabel = "";
    friendDef.uiDescription = "Get reassurance from Max the hedgehog";
    friendDef.factory = [this](const String& configValue) -> lv_obj_t* {
        // Create new friend card (ignore configValue for now)
        animationCard = new FriendCard(screen);
        
        if (animationCard && animationCard->getCard()) {
            // Register as input handler
            cardStack->registerInputHandler(animationCard->getCard(), animationCard);
            return animationCard->getCard();
        }
        
        delete animationCard;
        animationCard = nullptr;
        return nullptr;
    };
    registerCardType(friendDef);
}

void CardController::handleCardConfigChanged() {
    // Load new configuration from storage
    std::vector<CardConfig> newConfigs = configManager.getCardConfigs();
    
    // Perform reconciliation
    reconcileCards(newConfigs);
    
    // Update current configuration
    currentCardConfigs = newConfigs;
}

void CardController::reconcileCards(const std::vector<CardConfig>& newConfigs) {
    if (!displayInterface || !displayInterface->takeMutex(portMAX_DELAY)) {
        Serial.println("Failed to take mutex for card reconciliation");
        return;
    }
    
    // For now, implement a simple approach: remove all dynamic cards and recreate
    // TODO: Implement smarter diffing to preserve existing cards when possible
    
    // Remove existing insight cards
    for (auto* card : insightCards) {
        if (card && card->getCard()) {
            cardStack->removeCard(card->getCard());
        }
        delete card;
    }
    insightCards.clear();
    
    // Remove existing animation card
    if (animationCard && animationCard->getCard()) {
        cardStack->removeCard(animationCard->getCard());
        delete animationCard;
        animationCard = nullptr;
    }
    
    // Create cards based on new configuration, sorted by order
    std::vector<CardConfig> sortedConfigs = newConfigs;
    std::sort(sortedConfigs.begin(), sortedConfigs.end(), 
              [](const CardConfig& a, const CardConfig& b) {
                  return a.order < b.order;
              });
    
    for (const CardConfig& config : sortedConfigs) {
        // Find the registered card type
        auto it = std::find_if(registeredCardTypes.begin(), registeredCardTypes.end(),
                              [&config](const CardDefinition& def) {
                                  return def.type == config.type;
                              });
        
        if (it != registeredCardTypes.end() && it->factory) {
            // Create the card using the factory function
            lv_obj_t* cardObj = it->factory(config.config);
            if (cardObj) {
                cardStack->addCard(cardObj);
                Serial.printf("Created card of type %s with config: %s\n", 
                             cardTypeToString(config.type).c_str(), config.config.c_str());
            } else {
                Serial.printf("Failed to create card of type %s\n", 
                             cardTypeToString(config.type).c_str());
            }
        } else {
            Serial.printf("No factory found for card type %s\n", 
                         cardTypeToString(config.type).c_str());
        }
    }
    
    displayInterface->giveMutex();
} 