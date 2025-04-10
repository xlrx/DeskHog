#ifndef CAPTIVE_PORTAL_H
#define CAPTIVE_PORTAL_H

#include <Arduino.h>
#include <WebServer.h>
#include "ConfigManager.h"
#include "hardware/WifiInterface.h"
#include "html_portal.h"  // Include generated HTML header

class CaptivePortal {
public:
    // Constructor
    CaptivePortal(ConfigManager& configManager, WiFiInterface& wifiInterface);

    // Initialize the captive portal
    void begin();

    // Process incoming requests
    void process();

private:
    // Web server for captive portal
    WebServer _server;

    // Config manager reference
    ConfigManager& _configManager;

    // WiFi manager reference
    WiFiInterface& _wifiInterface;

    // Store the last scan result
    String _cachedNetworks;

    // Track when we last did a scan
    unsigned long _lastScanTime;

    // Handle main portal page
    void handleRoot();
    
    // Handle scan networks request
    void handleScanNetworks();
    
    // Handle WiFi configuration form
    void handleSaveWifi();

    // Handle device configuration
    void handleGetDeviceConfig();
    void handleSaveDeviceConfig();

    // Handle insights management
    void handleGetInsights();
    void handleSaveInsight();
    void handleDeleteInsight();

    // Handle captive portal detection
    void handleCaptivePortal();

    // Handle 404 Not Found
    void handle404();

    // Scan available networks
    String getNetworksJson();

    // New method to handle scanning
    void performWiFiScan();
};

#endif // CAPTIVE_PORTAL_H