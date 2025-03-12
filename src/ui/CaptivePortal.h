#ifndef CAPTIVE_PORTAL_H
#define CAPTIVE_PORTAL_H

#include <Arduino.h>
#include <WebServer.h>
#include "ConfigManager.h"
#include "hardware/WifiInterface.h"

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

    // Handle root page
    void handleRoot();

    // Handle WiFi configuration form
    void handleWifiConfig();

    // Handle captive portal detection
    void handleCaptivePortal();

    // Handle 404 Not Found
    void handle404();

    // Scan available networks
    String getScanNetworksHtml();

    // HTTP response helpers
    void sendHeader(const String& title);
    void sendFooter();
    String getRedirectScript(const String& url, int delay);
};

#endif // CAPTIVE_PORTAL_H