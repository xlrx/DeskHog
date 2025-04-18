#pragma once

#include <Arduino.h>
#include <WebServer.h>
#include "ConfigManager.h"
#include "hardware/WifiInterface.h"
#include "html_portal.h"  // Include generated HTML header
#include "EventQueue.h"   // Include the event queue

/**
 * @class CaptivePortal
 * @brief Web-based configuration interface for device setup
 * 
 * Provides a captive portal interface for:
 * - WiFi network selection and configuration
 * - Device configuration (team ID and API key)
 * - PostHog insight management
 * 
 * Implements standard captive portal detection for Android and Microsoft devices.
 * Caches WiFi scan results to improve responsiveness.
 */
class CaptivePortal {
public:
    /**
     * @brief Constructor
     * @param configManager Reference to configuration storage
     * @param wifiInterface Reference to WiFi management
     * @param eventQueue Reference to event system for state changes
     */
    CaptivePortal(ConfigManager& configManager, WiFiInterface& wifiInterface, EventQueue& eventQueue);

    /**
     * @brief Initialize the portal
     * 
     * Sets up HTTP routes and starts the web server on port 80.
     * Performs initial WiFi network scan.
     */
    void begin();

    /**
     * @brief Process incoming HTTP requests
     * 
     * Should be called regularly in the main loop.
     */
    void process();

private:
    WebServer _server;              ///< Web server instance on port 80
    ConfigManager& _configManager;   ///< Configuration storage reference
    WiFiInterface& _wifiInterface;   ///< WiFi management reference
    EventQueue& _eventQueue;         ///< Event system reference
    String _cachedNetworks;         ///< Cached JSON of available networks
    unsigned long _lastScanTime;     ///< Timestamp of last WiFi scan

    /**
     * @brief Serve the main portal page
     * Sends static HTML from html_portal.h
     */
    void handleRoot();
    
    /**
     * @brief Handle network scan request
     * Returns cached results and triggers new scan if >10s old
     */
    void handleScanNetworks();
    
    /**
     * @brief Handle WiFi credentials submission
     * Accepts POST with ssid/password, attempts connection
     */
    void handleSaveWifi();

    /**
     * @brief Return current device configuration
     * Returns team ID and truncated API key
     */
    void handleGetDeviceConfig();

    /**
     * @brief Handle device configuration updates
     * Accepts team ID and API key updates
     */
    void handleSaveDeviceConfig();

    /**
     * @brief Return list of configured insights
     * Returns JSON array of insight IDs and titles
     */
    void handleGetInsights();

    /**
     * @brief Handle new insight creation
     * Accepts insight ID, publishes INSIGHT_ADDED event
     */
    void handleSaveInsight();

    /**
     * @brief Handle insight deletion
     * Accepts JSON with insight ID, publishes INSIGHT_DELETED event
     */
    void handleDeleteInsight();

    /**
     * @brief Handle captive portal detection
     * Redirects to setup page for Android/Microsoft detection
     */
    void handleCaptivePortal();

    /**
     * @brief Handle 404 errors
     * Redirects all unknown URLs to setup page
     */
    void handle404();

    /**
     * @brief Scan and format available networks
     * @return JSON string of networks sorted by signal strength
     */
    String getNetworksJson();

    /**
     * @brief Perform WiFi network scan
     * Updates _cachedNetworks and _lastScanTime
     */
    void performWiFiScan();
};