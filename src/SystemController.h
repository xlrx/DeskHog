enum class SystemState {
    BOOT_COMPLETE,
    AWAITING_WIFI,
    WIFI_CONNECTED,
    AWAITING_API_CONFIGURATION,
    API_CONFIGURATION_INVALID,
    AWAITING_LOGIN_CONFIRMATION,
    LOGIN_CONFIRMATION_PROVIDED,
    INSIGHTS_CHANGED,
    SYSTEM_READY
};

#include <Arduino.h>
#include <functional>
#include <vector>

typedef std::function<void(SystemState)> StateChangeCallback;

class SystemController {
    private:
        SystemState state;
        static SystemController* instance;
        std::vector<StateChangeCallback> stateChangeCallbacks;
        
        // Private constructor for singleton
        SystemController();

        // Instance methods for internal use
        SystemState getStateInternal() const;
        void setStateInternal(SystemState newState);

                // Delete copy constructor and assignment operator
        SystemController(const SystemController&) = delete;
        void operator=(const SystemController&) = delete;
        
        // Singleton access method
        static SystemController* getInstance();
        
    public:
        
        // State management (static for convenience)
        static SystemState getState();
        static void setState(SystemState newState);
    
        
        // Callback management
        void onStateChange(StateChangeCallback callback);
        void removeAllCallbacks();
};  