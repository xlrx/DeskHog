#include "SystemController.h"

// Initialize static member
SystemController* SystemController::instance = nullptr;

// Private constructor
SystemController::SystemController() : state(SystemState::BOOT_COMPLETE) {
    // Initialize with BOOT_COMPLETE state
}

// Singleton access method
SystemController* SystemController::getInstance() {
    if (instance == nullptr) {
        instance = new SystemController();
    }
    return instance;
}

// Static state access method
SystemState SystemController::getState() {
    return getInstance()->getStateInternal();
}

// Static state setter method
void SystemController::setState(SystemState newState) {
    getInstance()->setStateInternal(newState);
}

// Get current system state (internal instance method)
SystemState SystemController::getStateInternal() const {
    return state;
}

// Set system state and trigger callbacks (internal instance method)
void SystemController::setStateInternal(SystemState newState) {
    // Only trigger callbacks if state actually changes
    if (state != newState) {
        state = newState;
        
        // Notify all registered callbacks
        for (const auto& callback : stateChangeCallbacks) {
            callback(state);
        }
        
        // Log state change
        Serial.print("System state changed to: ");
        switch (state) {
            case SystemState::BOOT_COMPLETE:
                Serial.println("BOOT_COMPLETE");
                break;
            case SystemState::AWAITING_WIFI:
                Serial.println("AWAITING_WIFI");
                break;
            case SystemState::WIFI_CONNECTED:
                Serial.println("WIFI_CONNECTED");
                break;
            case SystemState::AWAITING_API_CONFIGURATION:
                Serial.println("AWAITING_API_CONFIGURATION");
                break;
            case SystemState::API_CONFIGURATION_INVALID:
                Serial.println("API_CONFIGURATION_INVALID");
                break;
            case SystemState::AWAITING_LOGIN_CONFIRMATION:
                Serial.println("AWAITING_LOGIN_CONFIRMATION");
                break;
            case SystemState::LOGIN_CONFIRMATION_PROVIDED:
                Serial.println("LOGIN_CONFIRMATION_PROVIDED");
                break;
            case SystemState::INSIGHTS_CHANGED:
                Serial.println("INSIGHTS_CHANGED");
                break;
            case SystemState::SYSTEM_READY:
                Serial.println("SYSTEM_READY");
                break;
        }
    }
}

// Register a callback for state changes
void SystemController::onStateChange(StateChangeCallback callback) {
    stateChangeCallbacks.push_back(callback);
    
    // Immediately call the callback with the current state
    // This ensures newly registered components get the current state right away
    callback(state);
}

// Remove all callbacks
void SystemController::removeAllCallbacks() {
    stateChangeCallbacks.clear();
}
