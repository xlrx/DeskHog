#include <Arduino.h>
#include <lvgl.h>
#include <Adafruit_ST7789.h>
#include "ConfigManager.h"
#include "hardware/WifiInterface.h"
#include "ui/CaptivePortal.h"
#include "ui/ProvisioningCard.h"
#include "hardware/DisplayInterface.h"
#include "ui/CardNavigationStack.h"
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

// Task handles
TaskHandle_t wifiTask;
TaskHandle_t portalTask;
TaskHandle_t buttonTask;

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

void setup() {
    Serial.begin(115200);
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
    
    // Create card navigation stack with 2 cards
    cardStack = new CardNavigationStack(lv_scr_act(), SCREEN_WIDTH, SCREEN_HEIGHT, 2);
    
    // Create provision UI
    provisioningCard = new ProvisioningCard(
        lv_scr_act(), 
        *wifiInterface, 
        SCREEN_WIDTH, 
        SCREEN_HEIGHT
    );
    
    // Add provisioning card to navigation stack
    cardStack->addCard(provisioningCard->getCard());
    
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