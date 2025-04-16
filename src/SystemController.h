#pragma once

#include <Arduino.h>
#include <functional>
#include <vector>
#include "hardware/WifiInterface.h"

// Use the WiFiInterface's state enum
using WifiState = WiFiState;

enum class ApiState {
    API_NONE,
    API_AWAITING_CONFIG,
    API_CONFIG_INVALID,
    API_CONFIGURED
};

enum class AuthState {
    AUTH_NONE,
    AUTH_AWAITING_LOGIN,
    AUTH_CONFIRMED
};

enum class SystemState {
    SYS_BOOTING,
    SYS_READY,
    SYS_IDLE,
    SYS_INSIGHTS_CHANGED
};

struct ControllerState {
    WifiState wifi_state;
    ApiState api_state;
    AuthState auth_state;
    SystemState sys_state;
};

typedef std::function<void(const ControllerState&)> StateChangeCallback;

class SystemController {
private:
    ControllerState state;
    static SystemController* instance;
    std::vector<StateChangeCallback> state_change_callbacks;
    
    // Private constructor for singleton
    SystemController();

    // Delete copy constructor and assignment operator
    SystemController(const SystemController&) = delete;
    void operator=(const SystemController&) = delete;
    
    // Instance methods for internal use
    void notify_state_change();
    
    // Singleton access method
    static SystemController* getInstance();
    
    // WiFi event handler
    static void onWiFiStateChange(WiFiState new_state);
    
public:
    // Initialize the controller
    static void begin();
    
    // State accessors (static for convenience)
    static WifiState getWifiState();
    static ApiState getApiState();
    static AuthState getAuthState();
    static SystemState getSystemState();
    static ControllerState getFullState();
    
    // State setters (except WiFi which is managed by WiFiInterface)
    static void setApiState(ApiState new_state);
    static void setAuthState(AuthState new_state);
    static void setSystemState(SystemState new_state);
    
    // Callback management - now static
    static void onStateChange(StateChangeCallback callback);
    static void removeAllCallbacks();
    
    // Utility methods
    static bool isSystemFullyReady();
};  