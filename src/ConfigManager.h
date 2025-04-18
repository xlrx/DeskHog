#pragma once

#include <Arduino.h>
#include <Preferences.h>
#include <vector>
#include "EventQueue.h"

class ConfigManager {
public:
    static const int NO_TEAM_ID = -1;  // Sentinel value for no team ID

    // Constructor
    ConfigManager();

    // Constructor with EventQueue
    ConfigManager(EventQueue& eventQueue);

    // Initialize the config manager
    void begin();

    // Set the event queue (if not set in constructor)
    void setEventQueue(EventQueue* queue);

    // Save WiFi credentials
    bool saveWiFiCredentials(const String& ssid, const String& password);

    // Get WiFi credentials
    bool getWiFiCredentials(String& ssid, String& password);

    // Clear WiFi credentials
    void clearWiFiCredentials();

    // Check if WiFi credentials exist
    bool hasWiFiCredentials();

    // Check if WiFi credentials exist and publish appropriate event
    bool checkWiFiCredentialsAndPublish();

    // Team ID management
    void setTeamId(int teamId);
    int getTeamId();  // Returns NO_TEAM_ID if not set
    void clearTeamId();

    // API Key management
    bool setApiKey(const String& apiKey);
    String getApiKey();  // Returns empty string if not set
    void clearApiKey();

    // Insight management
    bool saveInsight(const String& id, const String& title);
    String getInsight(const String& id);
    void deleteInsight(const String& id);
    std::vector<String> getAllInsightIds();

private:
    // Helper method to update the insight ID list
    void updateIdList(const std::vector<String>& ids);
    
    // Helper method to check and update API configuration state
    void updateApiConfigurationState();

    // Helper method to commit changes to flash
    void commit();

    // Preferences instances for persistent storage
    Preferences _preferences;
    Preferences _insightsPrefs;

    // Namespace for WiFi credentials
    const char* _namespace = "wifi_config";
    const char* _insightsNamespace = "insights";

    // Keys for SSID and password
    const char* _ssidKey = "ssid";
    const char* _passwordKey = "password";
    const char* _hasCredentialsKey = "has_creds";

    // Keys for team ID and API key
    const char* _teamIdKey = "team_id";
    const char* _apiKeyKey = "api_key";

    // Max lengths for credentials
    static const size_t MAX_SSID_LENGTH = 32;
    static const size_t MAX_PASSWORD_LENGTH = 64;
    static const size_t MAX_INSIGHT_LENGTH = 1024;
    static const size_t MAX_API_KEY_LENGTH = 64;  // Added reasonable limit for API key
    static const size_t MAX_INSIGHT_ID_LENGTH = 64;  // Added reasonable limit for insight IDs

    // Event queue reference
    EventQueue* _eventQueue = nullptr;
};