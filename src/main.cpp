#include <Arduino.h>
#include <lvgl.h>
#include <Adafruit_ST7789.h>
#include "ConfigManager.h"
#include "hardware/WifiInterface.h"
#include "ui/CaptivePortal.h"
#include "ui/ProvisioningCard.h"
#include "hardware/DisplayInterface.h"

// Display dimensions
#define SCREEN_WIDTH 240
#define SCREEN_HEIGHT 135

// LVGL display buffer size
#define LVGL_BUFFER_ROWS 135  // Full screen height

// Global objects
DisplayInterface* display_manager;
ConfigManager* config_manager;
WiFiInterface* wifi_interface;
CaptivePortal* captive_portal;
ProvisioningCard* provisioning_card;

// Task handles
TaskHandle_t wifiTask;
TaskHandle_t portalTask;

// WiFi connection timeout in milliseconds
#define WIFI_TIMEOUT 30000

// WiFi task that handles WiFi operations
void wifiTaskFunction(void* parameter) {
    while (1) {
        // Process WiFi events
        wifi_interface->process();
        
        // Delay to prevent hogging CPU
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

// Captive portal task that handles web server requests
void portalTaskFunction(void* parameter) {
    while (1) {
        // Process portal requests regardless of mode
        captive_portal->process();
        
        // Delay to prevent hogging CPU
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

void setup() {
    Serial.begin(115200);
    Serial.println("Starting up...");
    
    // Initialize config manager
    config_manager = new ConfigManager();
    config_manager->begin();
    
    // Initialize display manager
    display_manager = new DisplayInterface(
        SCREEN_WIDTH, SCREEN_HEIGHT, LVGL_BUFFER_ROWS, 
        TFT_CS, TFT_DC, TFT_RST, TFT_BACKLITE
    );
    display_manager->begin();
    
    // Initialize WiFi manager
    wifi_interface = new WiFiInterface(*config_manager);
    wifi_interface->begin();
    
    // Create provision UI
    provisioning_card = new ProvisioningCard(
        lv_scr_act(), 
        *wifi_interface, 
        SCREEN_WIDTH, 
        SCREEN_HEIGHT
    );
    
    // Connect WiFi manager to UI
    wifi_interface->setUI(provisioning_card);
    
    // Initialize captive portal
    captive_portal = new CaptivePortal(*config_manager, *wifi_interface);
    captive_portal->begin();
    
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
                display_manager->handleLVGLTasks();
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
    if (config_manager->hasWiFiCredentials()) {
        Serial.println("Found WiFi credentials, attempting to connect...");
        
        // Try to connect to WiFi with stored credentials
        if (wifi_interface->connectToStoredNetwork(WIFI_TIMEOUT)) {
            provisioning_card->updateConnectionStatus("Connecting to WiFi...");
            provisioning_card->showWiFiStatus();
        } else {
            Serial.println("Failed to connect with stored credentials");
            // Start AP mode
            wifi_interface->startAccessPoint();
        }
    } else {
        Serial.println("No WiFi credentials found, starting AP mode");
        // Start AP mode
        wifi_interface->startAccessPoint();
    }
}

void loop() {
    // Empty - tasks handle everything
    vTaskDelete(NULL);
}