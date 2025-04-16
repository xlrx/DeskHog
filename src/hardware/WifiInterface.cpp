#include "WifiInterface.h"
#include "ui/ProvisioningCard.h"

WiFiInterface* WiFiInterface::_instance = nullptr;
WiFiStateCallback WiFiInterface::_stateCallback = nullptr;



WiFiInterface::WiFiInterface(ConfigManager& configManager, EventQueue& eventQueue)
    : _configManager(configManager),
      _eventQueue(&eventQueue),
      _state(WiFiState::DISCONNECTED),
      _apSSID("DeskHog_Setup"),
      _apPassword(""),
      _apIP(192, 168, 4, 1),
      _dnsServer(nullptr),
      _ui(nullptr),
      _lastStatusCheck(0),
      _connectionStartTime(0),
      _connectionTimeout(0) {
    
    _instance = this;
}

void WiFiInterface::setEventQueue(EventQueue* queue) {
    _eventQueue = queue;
}

void WiFiInterface::handleWiFiCredentialEvent(const Event& event) {
    if (event.type == EventType::WIFI_CREDENTIALS_FOUND) {
        Serial.println("WiFi credentials found event received");
        connectToStoredNetwork(30000); // 30 second timeout
    } else if (event.type == EventType::NEED_WIFI_CREDENTIALS) {
        Serial.println("Need WiFi credentials event received");
        startAccessPoint();
    }
}

void WiFiInterface::updateState(WiFiState newState) {
    if (_state == newState) return; // No change
    
    _state = newState;
    
    // Trigger legacy callback for backward compatibility
    if (_stateCallback) {
        _stateCallback(_state);
    }
    
    // Publish appropriate event
    if (_eventQueue != nullptr) {
        switch (newState) {
            case WiFiState::CONNECTING:
                _eventQueue->publishEvent(EventType::WIFI_CONNECTING, "");
                break;
            case WiFiState::CONNECTED:
                _eventQueue->publishEvent(EventType::WIFI_CONNECTED, "");
                break;
            case WiFiState::DISCONNECTED:
                if (!_configManager.hasWiFiCredentials()) {
                    _eventQueue->publishEvent(EventType::NEED_WIFI_CREDENTIALS, "");
                }
                break;
            case WiFiState::AP_MODE:
                _eventQueue->publishEvent(EventType::WIFI_AP_STARTED, "");
                break;
        }
    }
}

void WiFiInterface::onStateChange(WiFiStateCallback callback) {
    _stateCallback = callback;
    // Call immediately with current state if we have an instance
    if (_instance) {
        callback(_instance->_state);
    }
}

void WiFiInterface::begin() {
    // Set up WiFi event handlers
    WiFi.onEvent(onWiFiEvent);
    
    // Initialize WiFi
    WiFi.mode(WIFI_STA);
    
    // Subscribe to WiFi credential events if event queue is available
    if (_eventQueue != nullptr) {
        _eventQueue->subscribe([this](const Event& event) {
            if (event.type == EventType::WIFI_CREDENTIALS_FOUND || 
                event.type == EventType::NEED_WIFI_CREDENTIALS) {
                this->handleWiFiCredentialEvent(event);
            }
        });
    }
}

bool WiFiInterface::connectToStoredNetwork(unsigned long timeout) {
    if (!_configManager.getWiFiCredentials(_ssid, _password)) {
        return false;
    }
    
    Serial.print("Connecting to WiFi: ");
    Serial.println(_ssid);
    
    updateState(WiFiState::CONNECTING);
    _connectionStartTime = millis();
    _connectionTimeout = timeout;
    
    if (_ui) {
        _ui->updateConnectionStatus("Connecting");
    }
    
    // Start connection attempt
    WiFi.begin(_ssid.c_str(), _password.c_str());
    
    return true;
}

void WiFiInterface::startAccessPoint() {
    WiFi.mode(WIFI_AP);
    
    // Create unique AP SSID based on MAC address
    uint8_t mac[6];
    WiFi.macAddress(mac);
    _apSSID = "DeskHog_Setup_" + String(mac[2], HEX) + String(mac[3], HEX) + String(mac[4], HEX) + String(mac[5], HEX);
    
    // Start access point
    WiFi.softAPConfig(_apIP, _apIP, IPAddress(255, 255, 255, 0));
    WiFi.softAP(_apSSID.c_str(), _apPassword.c_str());
    
    // Start DNS server for captive portal
    if (!_dnsServer) {
        _dnsServer = new DNSServer();
        _dnsServer->start(53, "*", _apIP);
    }
    
    updateState(WiFiState::AP_MODE);
    
    if (_ui) {
        _ui->showQRCode();
    }
    
    Serial.print("AP started with SSID: ");
    Serial.println(_apSSID);
    Serial.print("AP IP address: ");
    Serial.println(getAPIPAddress());
}

void WiFiInterface::stopAccessPoint() {
    // Stop DNS server
    if (_dnsServer) {
        _dnsServer->stop();
        delete _dnsServer;
        _dnsServer = nullptr;
    }
    
    // Stop AP mode
    WiFi.softAPdisconnect(true);
    WiFi.mode(WIFI_STA);
    
    updateState(WiFiState::DISCONNECTED);
}

void WiFiInterface::process() {
    // Process DNS requests if in AP mode
    if (_state == WiFiState::AP_MODE && _dnsServer) {
        _dnsServer->processNextRequest();
    }
    
    // Check WiFi status periodically when connecting
    if (_state == WiFiState::CONNECTING) {
        unsigned long now = millis();
        
        // Check for timeout
        if (now - _connectionStartTime >= _connectionTimeout) {
            Serial.println("WiFi connection timeout");
            WiFi.disconnect();
            updateState(WiFiState::DISCONNECTED);
            
            if (_ui) {
                _ui->updateConnectionStatus("Connection failed: timeout");
            }
            
            // Publish connection failed event
            if (_eventQueue != nullptr) {
                _eventQueue->publishEvent(EventType::WIFI_CONNECTION_FAILED, "");
            }
            
            // Start AP mode for provisioning
            startAccessPoint();
        }
    }
    
    // Update signal strength periodically if connected
    if (_state == WiFiState::CONNECTED && millis() - _lastStatusCheck > 5000) {
        _lastStatusCheck = millis();
        
        if (_ui) {
            _ui->updateSignalStrength(getSignalStrength());
        }
    }
}

WiFiState WiFiInterface::getState() const {
    return _state;
}

String WiFiInterface::getIPAddress() const {
    if (_state == WiFiState::CONNECTED) {
        return WiFi.localIP().toString();
    }
    return "";
}

int WiFiInterface::getSignalStrength() const {
    if (_state == WiFiState::CONNECTED) {
        // Convert RSSI to percentage (typical range: -100 to -30)
        int rssi = WiFi.RSSI();
        if (rssi <= -100) {
            return 0;
        } else if (rssi >= -50) {
            return 100;
        } else {
            return 2 * (rssi + 100);
        }
    }
    return 0;
}

String WiFiInterface::getAPIPAddress() const {
    if (_state == WiFiState::AP_MODE) {
        return WiFi.softAPIP().toString();
    }
    return "";
}

String WiFiInterface::getSSID() const {
    if (_state == WiFiState::CONNECTED) {
        return _ssid;
    } else if (_state == WiFiState::AP_MODE) {
        return _apSSID;
    }
    return "";
}

void WiFiInterface::setUI(ProvisioningCard* ui) {
    _ui = ui;
}

void WiFiInterface::onWiFiEvent(WiFiEvent_t event) {
    if (!_instance) return;
    
    switch (event) {
        case ARDUINO_EVENT_WIFI_STA_CONNECTED:
            Serial.println("WiFi connected");
            _instance->updateState(WiFiState::CONNECTED);
            break;
            
        case ARDUINO_EVENT_WIFI_STA_GOT_IP:
            Serial.print("WiFi connected, IP address: ");
            Serial.println(WiFi.localIP());
            if (_instance->_ui) {
                _instance->_ui->updateConnectionStatus("Connected");
                _instance->_ui->updateIPAddress(WiFi.localIP().toString());
            }
            break;
            
        case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
            Serial.println("WiFi disconnected");
            if (_instance->_state == WiFiState::CONNECTED) {
                _instance->updateState(WiFiState::DISCONNECTED);
                
                if (_instance->_ui) {
                    _instance->_ui->updateConnectionStatus("Disconnected");
                }
            }
            break;
            
        default:
            break;
    }
}