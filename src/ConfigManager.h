#ifndef CONFIG_MANAGER_H
#define CONFIG_MANAGER_H

#include <Arduino.h>
#include <Preferences.h>

class ConfigManager {
public:
    // Constructor
    ConfigManager();

    // Initialize the config manager
    void begin();

    // Save WiFi credentials
    bool saveWiFiCredentials(const String& ssid, const String& password);

    // Get WiFi credentials
    bool getWiFiCredentials(String& ssid, String& password);

    // Clear WiFi credentials
    void clearWiFiCredentials();

    // Check if WiFi credentials exist
    bool hasWiFiCredentials();

private:
    // Preferences instance for persistent storage
    Preferences _preferences;

    // Namespace for WiFi credentials
    const char* _namespace = "wifi_config";

    // Keys for SSID and password
    const char* _ssidKey = "ssid";
    const char* _passwordKey = "password";
    const char* _hasCredentialsKey = "has_creds";

    // Max lengths for credentials
    static const size_t MAX_SSID_LENGTH = 32;
    static const size_t MAX_PASSWORD_LENGTH = 64;
};

#endif // CONFIG_MANAGER_H