#include "CaptivePortal.h"
#include <ArduinoJson.h>
#include "OtaManager.h"

CaptivePortal::CaptivePortal(ConfigManager& configManager, WiFiInterface& wifiInterface, EventQueue& eventQueue)
    : _server(80), _configManager(configManager), _wifiInterface(wifiInterface), _eventQueue(eventQueue), _lastScanTime(0) {
    _otaManager = new OtaManager(CURRENT_FIRMWARE_VERSION, "PostHog", "DeskHog");
    _cachedNetworks = "{\"networks\":[]}"; // Initialize with empty valid JSON
}

void CaptivePortal::begin() {
    // Set up server routes
    _server.on("/", HTTP_GET, [this]() { handleRoot(); });
    _server.on("/scan-networks", HTTP_GET, [this]() { handleScanNetworks(); });
    _server.on("/save-wifi", HTTP_POST, [this]() { handleSaveWifi(); });
    _server.on("/get-device-config", HTTP_GET, [this]() { handleGetDeviceConfig(); });
    _server.on("/save-device-config", HTTP_POST, [this]() { handleSaveDeviceConfig(); });
    _server.on("/get-insights", HTTP_GET, [this]() { handleGetInsights(); });
    _server.on("/save-insight", HTTP_POST, [this]() { handleSaveInsight(); });
    _server.on("/delete-insight", HTTP_POST, [this]() { handleDeleteInsight(); });
    _server.on("/generate_204", HTTP_GET, [this]() { handleCaptivePortal(); }); // Android captive portal detection
    _server.on("/fwlink", HTTP_GET, [this]() { handleCaptivePortal(); }); // Microsoft captive portal detection

    // OTA Routes
    _server.on("/check-update", HTTP_GET, [this]() { handleCheckUpdate(); });
    _server.on("/check-update", HTTP_OPTIONS, [this]() { handleCorsPreflight(); });
    _server.on("/start-update", HTTP_POST, [this]() { handleStartUpdate(); });
    _server.on("/start-update", HTTP_OPTIONS, [this]() { handleCorsPreflight(); });
    _server.on("/update-status", HTTP_GET, [this]() { handleUpdateStatus(); });
    _server.on("/update-status", HTTP_OPTIONS, [this]() { handleCorsPreflight(); });
    
    // Handle all other routes as a captive portal
    _server.onNotFound([this]() { handle404(); });
    
    // Start server
    _server.begin();
    
    Serial.println("Captive portal started");
}

void CaptivePortal::process() {
    _server.handleClient();
}

void CaptivePortal::handleRoot() {
    // Return the static HTML - network list will be populated by JavaScript
    String html = FPSTR(PORTAL_HTML);
    _server.send(200, "text/html", html);
}

void CaptivePortal::handleScanNetworks() {
    // Return cached networks immediately
    _server.send(200, "application/json", _cachedNetworks);
    
    // Trigger a new scan for next time if it's been more than 10 seconds
    unsigned long now = millis();
    if (now - _lastScanTime >= 10000) {  // 10 second cooldown
        performWiFiScan();
    }
}

void CaptivePortal::performWiFiScan() {
    _lastScanTime = millis();
    _cachedNetworks = getNetworksJson();
}

void CaptivePortal::handleSaveWifi() {
    // This endpoint handles the AJAX POST from the form
    String ssid = _server.arg("ssid");
    String password = _server.arg("password");
    bool success = false;
    
    if (ssid.length() > 0) {
        success = _configManager.saveWiFiCredentials(ssid, password);
        
        if (success) {
            // Try to connect to the new network in the background
            _wifiInterface.stopAccessPoint();
            _wifiInterface.connectToStoredNetwork();
        }
    }
    
    // Return JSON response
    String response = "{\"success\":" + String(success ? "true" : "false") + "}";
    _server.send(200, "application/json", response);
}

void CaptivePortal::handleGetDeviceConfig() {
    // Create JSON response with current config
    StaticJsonDocument<256> doc;
    doc["teamId"] = _configManager.getTeamId();
    String truncatedKey = _configManager.getApiKey();
    if (truncatedKey.length() > 12) {
        truncatedKey = truncatedKey.substring(0, 12) + "...";
    }
    doc["apiKey"] = truncatedKey;
    
    String response;
    serializeJson(doc, response);
    _server.send(200, "application/json", response);
}

void CaptivePortal::handleSaveDeviceConfig() {
    bool success = true;
    String message;
    
    // Handle team ID
    if (_server.hasArg("teamId")) {
        int teamId = _server.arg("teamId").toInt();
        _configManager.setTeamId(teamId);
    }
    
    // Handle API key
    if (_server.hasArg("apiKey")) {
        String apiKey = _server.arg("apiKey");
        if (apiKey.endsWith("...")) {
            success = true;
            message = "Discarding truncated API key";
        } else if (!_configManager.setApiKey(apiKey)) {
            success = false;
            message = "Invalid API key";
        }
    }
    
    // Create JSON response
    StaticJsonDocument<128> doc;
    doc["success"] = success;
    if (!success) {
        doc["message"] = message;
    }
    
    String response;
    serializeJson(doc, response);
    _server.send(200, "application/json", response);
}

void CaptivePortal::handleGetInsights() {
    // Create JSON response with insights list
    DynamicJsonDocument doc(4096);  // Adjust size based on your needs
    JsonArray insights = doc.createNestedArray("insights");
    
    auto ids = _configManager.getAllInsightIds();
    for (const auto& id : ids) {
        JsonObject insight = insights.createNestedObject();
        insight["id"] = id;
        String title = _configManager.getInsight(id);
        insight["title"] = title.length() > 1 ? title : id;
    }
    
    String response;
    serializeJson(doc, response);
    _server.send(200, "application/json", response);
}

void CaptivePortal::handleSaveInsight() {
    bool success = false;
    String message;
    
    if (_server.hasArg("insightId")) {
        String id = _server.arg("insightId");
        
        success = _configManager.saveInsight(id, "");
        if (success) {
            // Publish an event that an insight was added
            _eventQueue.publishEvent(EventType::INSIGHT_ADDED, id);
        } else {
            message = "Failed to save insight";
        }
    } else {
        message = "Missing required fields";
    }
    
    // Create JSON response
    StaticJsonDocument<128> doc;
    doc["success"] = success;
    if (!success) {
        doc["message"] = message;
    }
    
    String response;
    serializeJson(doc, response);
    _server.send(200, "application/json", response);
}

void CaptivePortal::handleDeleteInsight() {
    bool success = false;
    String message;
    
    // Parse JSON request body
    StaticJsonDocument<128> doc;
    if (_server.hasArg("plain")) {
        DeserializationError error = deserializeJson(doc, _server.arg("plain"));
        
        if (!error) {
            const char* id = doc["id"];
            if (id) {
                _configManager.deleteInsight(id);
                success = true;
                
                // Publish an event that an insight was deleted
                _eventQueue.publishEvent(EventType::INSIGHT_DELETED, id);
            } else {
                message = "Missing insight ID";
            }
        } else {
            message = "Invalid JSON";
        }
    } else {
        message = "Missing request body";
    }
    
    // Create JSON response
    StaticJsonDocument<128> response;
    response["success"] = success;
    if (!success) {
        response["message"] = message;
    }
    
    String responseStr;
    serializeJson(response, responseStr);
    _server.send(200, "application/json", responseStr);
}

void CaptivePortal::handleCaptivePortal() {
    // Redirect to the setup page for captive portal detection
    _server.sendHeader("Location", "http://" + _wifiInterface.getAPIPAddress(), true);
    _server.send(302, "text/plain", "");
}

void CaptivePortal::handle404() {
    // Redirect to the setup page for all unknown URLs
    _server.sendHeader("Location", "http://" + _wifiInterface.getAPIPAddress(), true);
    _server.send(302, "text/plain", "");
}

String CaptivePortal::getNetworksJson() {
    DynamicJsonDocument doc(4096);  // Adjust size based on max networks
    JsonArray networks = doc.createNestedArray("networks");
    
    // Scan for networks
    int numNetworks = WiFi.scanNetworks();
    
    if (numNetworks > 0) {
        // Sort networks by signal strength
        int indices[numNetworks];
        for (int i = 0; i < numNetworks; i++) {
            indices[i] = i;
        }
        
        // Sort by RSSI (signal strength)
        for (int i = 0; i < numNetworks; i++) {
            for (int j = i + 1; j < numNetworks; j++) {
                if (WiFi.RSSI(indices[j]) > WiFi.RSSI(indices[i])) {
                    std::swap(indices[i], indices[j]);
                }
            }
        }
        
        // Add networks to JSON array
        for (int i = 0; i < numNetworks; i++) {
            int index = indices[i];
            String ssid = WiFi.SSID(index);
            int rssi = WiFi.RSSI(index);
            bool encrypted = WiFi.encryptionType(index) != WIFI_AUTH_OPEN;
            
            // Only include networks with an SSID
            if (ssid.length() > 0) {
                JsonObject network = networks.createNestedObject();
                network["ssid"] = ssid;
                network["rssi"] = rssi;
                network["encrypted"] = encrypted;
            }
        }
    }
    
    // Free memory used by scan
    WiFi.scanDelete();
    
    String response;
    serializeJson(doc, response);
    return response;
}

// Generic CORS preflight handler
void CaptivePortal::handleCorsPreflight() {
    Serial.println("CP DBG: handleCorsPreflight - Sending CORS preflight headers.");
    _server.sendHeader("Access-Control-Allow-Origin", "*");
    _server.sendHeader("Access-Control-Max-Age", "10000"); // Cache preflight for 10000 seconds
    _server.sendHeader("Access-Control-Allow-Methods", "POST,GET,OPTIONS");
    _server.sendHeader("Access-Control-Allow-Headers", "Origin, X-Requested-With, Content-Type, Accept");
    _server.send(204); // No Content
}

void CaptivePortal::handleCheckUpdate() {
    Serial.println("CP DBG: handleCheckUpdate - Entered.");
    if (!_otaManager) {
        _server.send(500, "application/json", "{\"error\":\"OTA manager not initialized\"}");
        return;
    }

    bool checkInitiated = _otaManager->checkForUpdate();
    
    UpdateStatus currentStatus = _otaManager->getStatus();
    // Get current version from OtaManager, not necessarily from last check result which might be old
    String currentFirmware = _otaManager->getLastCheckResult().currentVersion; 
    if (currentFirmware.isEmpty()) { // Fallback if not set yet in lastCheckResult
        currentFirmware = CURRENT_FIRMWARE_VERSION;
    }

    DynamicJsonDocument doc(256);
    doc["check_initiated"] = checkInitiated;
    doc["current_firmware_version"] = currentFirmware;
    // Send the status code as an integer
    doc["initial_status_code"] = static_cast<int>(currentStatus.status);
    doc["initial_status_message"] = currentStatus.message;
    if (!checkInitiated && currentStatus.status == UpdateStatus::State::IDLE) {
        // If check couldn't be initiated (e.g. WiFi off, or already checking),
        // and status is IDLE, use the last check error if available.
        // This helps propagate "WiFi not connected" type errors immediately.
        doc["initial_status_message"] = _otaManager->getLastCheckResult().error.isEmpty() ? currentStatus.message : _otaManager->getLastCheckResult().error;
    }


    String response;
    serializeJson(doc, response);
    Serial.printf("CP DBG: handleCheckUpdate - Sending response: %s\n", response.c_str());
    _server.sendHeader("Access-Control-Allow-Origin", "*");
    _server.send(200, "application/json", response);
}

void CaptivePortal::handleStartUpdate() {
    Serial.println("CP DBG: handleStartUpdate - Entered.");
    if (!_otaManager) {
        Serial.println("CP DBG: handleStartUpdate - OTA manager not initialized.");
        _server.send(500, "application/json", "{\"error\":\"OTA manager not initialized\"}");
        return;
    }

    String downloadUrl = _otaManager->getLastCheckResult().downloadUrl;
    Serial.printf("CP DBG: handleStartUpdate - URL from getLastCheckResult(): %s\n", downloadUrl.c_str());

    if (downloadUrl.isEmpty() && _otaManager->getLastCheckResult().updateAvailable) {
         Serial.println("CP DBG: handleStartUpdate - No download URL available from last check, but update is available.");
         _server.send(400, "application/json", "{\"success\":false, \"message\":\"No download URL available from last check.\"}");
        return;
    }
    // If no update is available according to last check, it doesn't make sense to proceed, even if a URL was somehow present.
    // This prevents trying to "update" to the same version or if check failed.
    if (!_otaManager->getLastCheckResult().updateAvailable) {
        Serial.println("CP DBG: handleStartUpdate - getLastCheckResult() shows no update available. Aborting start.");
        _server.send(400, "application/json", "{\"success\":false, \"message\":\"No update marked as available from the last check.\"}");
        return;
    }

    Serial.println("CP DBG: handleStartUpdate - Calling _otaManager->beginUpdate().");
    bool success = _otaManager->beginUpdate(downloadUrl); 
    Serial.printf("CP DBG: handleStartUpdate - _otaManager->beginUpdate() returned: %s\n", success ? "true" : "false");
    
    StaticJsonDocument<128> doc;
    doc["success"] = success;
    if (!success) {
        doc["message"] = _otaManager->getStatus().message; 
        Serial.printf("CP DBG: handleStartUpdate - Update start failed. Message: %s\n", _otaManager->getStatus().message.c_str());
    }
    
    String response;
    serializeJson(doc, response);
    Serial.printf("CP DBG: handleStartUpdate - Sending response: %s\n", response.c_str());
    _server.sendHeader("Access-Control-Allow-Origin", "*");
    _server.send(200, "application/json", response);
}

void CaptivePortal::handleUpdateStatus() {
    Serial.println("CP DBG: handleUpdateStatus - Entered."); // Added DBG log
    if (!_otaManager) {
        Serial.println("CP DBG: handleUpdateStatus - OTA manager not initialized."); // Added DBG log
        _server.sendHeader("Access-Control-Allow-Origin", "*");
        _server.send(500, "application/json", "{\"error\":\"OTA manager not initialized\"}");
        return;
    }
    
    UpdateStatus status = _otaManager->getStatus();
    UpdateInfo info = _otaManager->getLastCheckResult(); // This will be fresh after a check task completes
    
    DynamicJsonDocument doc(1024); // Increased size for combined info
    
    doc["status_code"] = static_cast<int>(status.status);
    doc["status_message"] = status.message;
    doc["progress"] = status.progress;
    
    // Add fields from UpdateInfo
    doc["current_firmware_version_info"] = info.currentVersion; // Renamed to avoid conflict
    doc["available_firmware_version_info"] = info.availableVersion;
    doc["is_update_available_info"] = info.updateAvailable;
    doc["release_notes_info"] = info.releaseNotes;
    doc["error_message_info"] = info.error; // Error specifically from the check process
    
    String response;
    serializeJson(doc, response);
    // Serial.printf("CP DBG: handleUpdateStatus - Sending response: %s\n", response.c_str()); // Optional: can be very verbose
    _server.sendHeader("Access-Control-Allow-Origin", "*");
    _server.send(200, "application/json", response);
}