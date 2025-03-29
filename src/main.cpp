#include <Arduino.h>
#include <lvgl.h>
#include <Adafruit_ST7789.h>
#include "ConfigManager.h"
#include "hardware/WifiInterface.h"
#include "ui/CaptivePortal.h"
#include "ui/ProvisioningCard.h"
#include "hardware/DisplayInterface.h"
#include "ui/CardNavigationStack.h"
#include "ui/InsightCard.h"
#include "hardware/Input.h"

// Display dimensions
#define SCREEN_WIDTH 240
#define SCREEN_HEIGHT 135

// LVGL display buffer size
#define LVGL_BUFFER_ROWS 135  // Full screen height

// Button configuration
#define NUM_BUTTONS 3
const uint8_t BUTTON_PINS[NUM_BUTTONS] = {
    Input::BUTTON_DOWN,    // BOOT button
    Input::BUTTON_CENTER,  // Center button
    Input::BUTTON_UP       // Up button
};

// Define the global buttons array
Bounce2::Button buttons[NUM_BUTTONS];

// Global objects
DisplayInterface* displayInterface;
ConfigManager* configManager;
WiFiInterface* wifiInterface;
CaptivePortal* captivePortal;
ProvisioningCard* provisioningCard;
CardNavigationStack* cardStack;
std::vector<InsightCard*> insightCards;  // Store pointers to insight cards

// Task handles
TaskHandle_t wifiTask;
TaskHandle_t portalTask;
TaskHandle_t buttonTask;
TaskHandle_t insightTask;

// WiFi connection timeout in milliseconds
#define WIFI_TIMEOUT 30000

// WiFi task that handles WiFi operations
void wifiTaskFunction(void* parameter) {
    while (1) {
        // Process WiFi events
        wifiInterface->process();
        
        // Delay to prevent hogging CPU
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

// Captive portal task that handles web server requests
void portalTaskFunction(void* parameter) {
    while (1) {
        // Process portal requests regardless of mode
        captivePortal->process();
        
        // Delay to prevent hogging CPU
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

// Insight processing task
void insightTaskFunction(void* parameter) {
    while (1) {
        // Take mutex before accessing cards
        if (displayInterface->takeMutex(pdMS_TO_TICKS(100))) {  // 100ms timeout
            // Process each card if it still exists
            for (auto it = insightCards.begin(); it != insightCards.end();) {
                InsightCard* card = *it;
                
                // Check if card is still valid
                if (card && card->getCard() && lv_obj_is_valid(card->getCard())) {
                    card->process();
                    ++it;
                } else {
                    // Card is invalid, remove it from the vector
                    it = insightCards.erase(it);
                }
            }
            
            // Release mutex
            displayInterface->giveMutex();
        }
        
        vTaskDelay(pdMS_TO_TICKS(100));  // Check every 100ms
    }
}

void setup() {
    Serial.begin(115200);
    delay(100);  // Give serial port time to initialize
    Serial.println("Starting up...");
    
    // Initialize config manager
    configManager = new ConfigManager();
    configManager->begin();
    
    // Initialize display manager
    displayInterface = new DisplayInterface(
        SCREEN_WIDTH, SCREEN_HEIGHT, LVGL_BUFFER_ROWS, 
        TFT_CS, TFT_DC, TFT_RST, TFT_BACKLITE
    );
    displayInterface->begin();
    
    // Initialize WiFi manager
    wifiInterface = new WiFiInterface(*configManager);
    wifiInterface->begin();
    
    // Initialize buttons
    Input::configureButtons();
    
    // Get count of insights to determine card count
    std::vector<String> insightIds = configManager->getAllInsightIds();
    int totalCards = insightIds.size() + 2;  // +2 for provisioning and metrics cards
    
    // Create card navigation stack
    cardStack = new CardNavigationStack(lv_scr_act(), SCREEN_WIDTH, SCREEN_HEIGHT, totalCards);
    
    // Create provision UI
    provisioningCard = new ProvisioningCard(
        lv_scr_act(), 
        *wifiInterface, 
        SCREEN_WIDTH, 
        SCREEN_HEIGHT
    );
    
    // Add provisioning card to navigation stack
    cardStack->addCard(provisioningCard->getCard());
    
    // Create and add insight cards
    int delay_ms = 0;
    const int DELAY_BETWEEN_INSIGHTS = 2000;  // 2 seconds between each insight
    
    for (const String& id : insightIds) {
        auto* insightCard = new InsightCard(
            lv_scr_act(),
            *configManager,
            id,
            SCREEN_WIDTH,
            SCREEN_HEIGHT,
            displayInterface
        );
        cardStack->addCard(insightCard->getCard());
        insightCards.push_back(insightCard);  // Store the pointer
        
        // Start processing with a staggered delay
        insightCard->startProcessing(delay_ms);
        delay_ms += DELAY_BETWEEN_INSIGHTS;
    }
    
    // Add a sample metrics card
    cardStack->addCard(lv_color_hex(0xe74c3c), "Metrics");
    
    // Connect WiFi manager to UI
    wifiInterface->setUI(provisioningCard);
    
    // Initialize captive portal
    captivePortal = new CaptivePortal(*configManager, *wifiInterface);
    captivePortal->begin();
    
    // Set mutex for thread safety
    cardStack->setMutex(displayInterface->getMutexPtr());
    
    // Create task for button handling
    xTaskCreatePinnedToCore(
        CardNavigationStack::buttonTask,
        "buttonTask",
        2048,
        NULL,
        1,
        &buttonTask,
        0
    );
    
    // Create task for WiFi operations
    xTaskCreatePinnedToCore(
        wifiTaskFunction,
        "wifiTask",
        4096,
        NULL,
        1,
        &wifiTask,
        0
    );
    
    // Create task for captive portal
    xTaskCreatePinnedToCore(
        portalTaskFunction,
        "portalTask",
        4096,
        NULL,
        1,
        &portalTask,
        0
    );
    
    // Create task for insight processing
    xTaskCreatePinnedToCore(
        insightTaskFunction,
        "insightTask",
        81920,
        NULL,
        1,
        &insightTask,
        0
    );
    
    // Create LVGL tick task
    xTaskCreatePinnedToCore(
        DisplayInterface::tickTask,
        "lv_tick_task",
        2048,
        NULL,
        2,
        NULL,
        1
    );
    
    // Create LVGL handler task
    xTaskCreatePinnedToCore(
        [](void *parameter) {
            while (1) {
                displayInterface->handleLVGLTasks();
                vTaskDelay(pdMS_TO_TICKS(5));
            }
        },
        "lvglTask",
        4096,
        NULL,
        2,
        NULL,
        1
    );
    
    // Check if we have WiFi credentials
    if (configManager->hasWiFiCredentials()) {
        Serial.println("Found WiFi credentials, attempting to connect...");
        
        // Try to connect to WiFi with stored credentials
        if (wifiInterface->connectToStoredNetwork(WIFI_TIMEOUT)) {
            provisioningCard->updateConnectionStatus("Connecting to WiFi...");
            provisioningCard->showWiFiStatus();
        } else {
            Serial.println("Failed to connect with stored credentials");
            // Start AP mode
            wifiInterface->startAccessPoint();
        }
    } else {
        Serial.println("No WiFi credentials found, starting AP mode");
        // Start AP mode
        wifiInterface->startAccessPoint();
    }
}

void loop() {
    // Empty - tasks handle everything
    vTaskDelete(NULL);
}