#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <Arduino.h>
#include <WiFi.h>
#include <DNSServer.h>
#include <WebServer.h>
#include "ConfigManager.h"
#include "EventQueue.h"
#include <functional>

// Forward declarations
class ProvisioningCard;

// WiFi connection states
enum class WiFiState {
    DISCONNECTED,
    CONNECTING,
    CONNECTED,
    AP_MODE
};

// State change callback type
typedef std::function<void(WiFiState)> WiFiStateCallback;

class WiFiInterface {
public:
    
    // Constructor with EventQueue
    WiFiInterface(ConfigManager& configManager, EventQueue& eventQueue);

    // Initialize the WiFi manager
    void begin();

    // Connect to WiFi using stored credentials
    bool connectToStoredNetwork(unsigned long timeout = 30000);

    // Start access point mode for provisioning
    void startAccessPoint();

    // Stop access point mode
    void stopAccessPoint();

    // Process WiFi events (call in loop)
    void process();

    // Get current WiFi state
    WiFiState getState() const;

    // Get current IP address (empty if not connected)
    String getIPAddress() const;

    // Get WiFi signal strength (0-100%, 0 if not connected)
    int getSignalStrength() const;

    // Get access point IP address
    String getAPIPAddress() const;
    
    // Get SSID
    String getSSID() const;

    // Set UI reference for callbacks
    void setUI(ProvisioningCard* ui);
    
    // Register for state changes
    static void onStateChange(WiFiStateCallback callback);
    
    // Set the event queue (if not set in constructor)
    void setEventQueue(EventQueue* queue);
    
    // Handle WiFi credential events
    void handleWiFiCredentialEvent(const Event& event);

private:
    // Config manager reference
    ConfigManager& _configManager;
    
    // Event queue reference
    EventQueue* _eventQueue = nullptr;

    // WiFi state
    WiFiState _state;

    // Stored credentials
    String _ssid;
    String _password;

    // AP mode settings
    String _apSSID;
    String _apPassword;
    IPAddress _apIP;

    // DNS server for captive portal
    DNSServer* _dnsServer;
    
    // UI reference for callbacks
    ProvisioningCard* _ui;

    // Last time we checked WiFi status
    unsigned long _lastStatusCheck;
    
    // Connection attempt start time
    unsigned long _connectionStartTime;
    
    // Timeout for connection attempt
    unsigned long _connectionTimeout;
    
    // WiFi event handlers
    static void onWiFiEvent(WiFiEvent_t event);
    static WiFiInterface* _instance;
    
    // State change callback
    static WiFiStateCallback _stateCallback;
    
    // Update WiFi state and publish event
    void updateState(WiFiState newState);
};

#endif // WIFI_MANAGER_H