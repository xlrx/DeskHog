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
#include "posthog/PostHogClient.h"
#include "Style.h"
#include "esp_heap_caps.h" // For PSRAM management
#include "ui/CardController.h" // Add CardController include
#include "EventQueue.h" // Add EventQueue include

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
CardController* cardController; // Replace individual card objects with controller
PostHogClient* posthogClient;
EventQueue* eventQueue; // Add global EventQueue

// Task handles
TaskHandle_t wifiTask;
TaskHandle_t portalTask;
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
        posthogClient->process();  // Use the global instance
        vTaskDelay(pdMS_TO_TICKS(100));  // Check every 100ms
    }
}

// LVGL handler task that includes button polling - added here to consolidate UI operations
void lvglHandlerTask(void* parameter) {
    TickType_t lastButtonCheck = xTaskGetTickCount();
    const TickType_t buttonCheckInterval = pdMS_TO_TICKS(50); // Check buttons every 50ms
    
    while (1) {
        // Handle LVGL tasks
        displayInterface->handleLVGLTasks();
        
        // Poll buttons at regular intervals
        TickType_t currentTime = xTaskGetTickCount();
        if ((currentTime - lastButtonCheck) >= buttonCheckInterval) {
            lastButtonCheck = currentTime;
            
            // Poll all buttons
            for (int i = 0; i < NUM_BUTTONS; i++) {
                buttons[i].update();
                
                if (buttons[i].pressed()) {
                    // Process button directly in LVGL context
                    cardController->getCardStack()->handleButtonPress(i);
                }
            }
        }
        
        vTaskDelay(pdMS_TO_TICKS(5));
    }
}

void setup() {
    Serial.begin(115200);
    delay(100);  // Give serial port time to initialize
    Serial.println("Starting up...");

    if (psramInit()) {
        Serial.println("PSRAM initialized successfully");
        Serial.printf("Total PSRAM: %d bytes\n", ESP.getPsramSize());
        Serial.printf("Free PSRAM: %d bytes\n", ESP.getFreePsram());
        
        // Set memory allocation preference to PSRAM
        heap_caps_malloc_extmem_enable(4096);  // Allow allocations less than 4096 bytes from PSRAM
    } else {
        Serial.println("PSRAM initialization failed!");
        while(1); // Stop here if PSRAM init fails
    }

    SystemController::begin();
    
    // Initialize styles and fonts
    Style::init();
    
    // Initialize event queue first
    eventQueue = new EventQueue(20); // Create queue with capacity for 20 events
    eventQueue->begin(); // Start event processing
    
    // Initialize config manager with event queue
    configManager = new ConfigManager(*eventQueue);
    configManager->begin();
    
    // Initialize PostHog client with event queue
    posthogClient = new PostHogClient(*configManager, *eventQueue);
    
    // Initialize display manager
    displayInterface = new DisplayInterface(
        SCREEN_WIDTH, SCREEN_HEIGHT, LVGL_BUFFER_ROWS, 
        TFT_CS, TFT_DC, TFT_RST, TFT_BACKLITE
    );
    displayInterface->begin();
    
    // Initialize WiFi manager with event queue
    wifiInterface = new WiFiInterface(*configManager, *eventQueue);
    wifiInterface->begin();
    
    // Initialize buttons
    Input::configureButtons();
    
    // Create and initialize card controller
    cardController = new CardController(
        lv_scr_act(),
        SCREEN_WIDTH,
        SCREEN_HEIGHT,
        *configManager,
        *wifiInterface,
        *posthogClient,
        *eventQueue
    );
    
    // Initialize with display interface directly
    cardController->initialize(displayInterface);
    
    // Initialize captive portal
    captivePortal = new CaptivePortal(*configManager, *wifiInterface, *eventQueue);
    captivePortal->begin();
    
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
        1,
        NULL,
        1
    );
    
    // Create LVGL handler task (now includes button polling)
    xTaskCreatePinnedToCore(
        lvglHandlerTask,
        "lvglTask",
        8192,
        NULL,
        2,
        NULL,
        1
    );
    
    // Check if we have WiFi credentials and publish the appropriate event
    configManager->checkWiFiCredentialsAndPublish();

    SystemController::setSystemState(SystemState::SYS_READY);
}

void loop() {
    // Empty - tasks handle everything
    vTaskDelete(NULL);
}