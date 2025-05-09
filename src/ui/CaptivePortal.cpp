#include "CaptivePortal.h"
#include <ArduinoJson.h>
#include "AsyncJson.h"
#include "OtaManager.h"
// ESPAsyncWebServer will be included via CaptivePortal.h

CaptivePortal::CaptivePortal(ConfigManager& configManager, WiFiInterface& wifiInterface, EventQueue& eventQueue)
    : _server(80), _configManager(configManager), _wifiInterface(wifiInterface), _eventQueue(eventQueue), _lastScanTime(0) {
    _otaManager = new OtaManager(CURRENT_FIRMWARE_VERSION, "PostHog", "DeskHog");
    _cachedNetworks = "{\"networks\":[]}"; // Initialize with empty valid JSON
}

void CaptivePortal::begin() {
    // Set global CORS header
    DefaultHeaders::Instance().addHeader("Access-Control-Allow-Origin", "*");

    // Set up server routes
    _server.on("/", HTTP_GET, [this](AsyncWebServerRequest *request) { handleRoot(request); });
    _server.on("/scan-networks", HTTP_GET, [this](AsyncWebServerRequest *request) { handleScanNetworks(request); });
    _server.on("/save-wifi", HTTP_POST, [this](AsyncWebServerRequest *request) { handleSaveWifi(request); });
    _server.on("/get-device-config", HTTP_GET, [this](AsyncWebServerRequest *request) { handleGetDeviceConfig(request); });
    _server.on("/save-device-config", HTTP_POST, [this](AsyncWebServerRequest *request) { handleSaveDeviceConfig(request); });
    _server.on("/get-insights", HTTP_GET, [this](AsyncWebServerRequest *request) { handleGetInsights(request); });
    _server.on("/save-insight", HTTP_POST, [this](AsyncWebServerRequest *request) { handleSaveInsight(request); });
    
    // Special handler for /delete-insight to parse JSON body
    AsyncCallbackJsonWebHandler* deleteInsightHandler = new AsyncCallbackJsonWebHandler("/delete-insight", 
        [this](AsyncWebServerRequest *request, JsonVariant &json) {
            bool success = false;
            String message;
            StaticJsonDocument<128> doc; // Doc for parsing the received JSON, if needed beyond direct JsonVariant access
            
            if (json.is<JsonObject>()) {
                JsonObject jsonObj = json.as<JsonObject>();
                if (jsonObj.containsKey("id") && jsonObj["id"].is<const char*>()) {
                    const char* id = jsonObj["id"].as<const char*>();
                    _configManager.deleteInsight(id);
                    success = true;
                    _eventQueue.publishEvent(EventType::INSIGHT_DELETED, id);
                } else {
                    message = "Missing or invalid insight ID in JSON body";
                }
            } else {
                message = "Invalid JSON body";
            }
            
            StaticJsonDocument<128> responseDoc;
            responseDoc["success"] = success;
            if (!message.isEmpty()) {
                responseDoc["message"] = message;
            }
            
            String responseStr;
            serializeJson(responseDoc, responseStr);
            request->send(200, "application/json", responseStr);
        }
    ); // Max JSON size can be specified as a third argument if needed, e.g., 1024
    _server.addHandler(deleteInsightHandler);

    _server.on("/generate_204", HTTP_GET, [this](AsyncWebServerRequest *request) { handleCaptivePortal(request); }); // Android captive portal detection
    _server.on("/fwlink", HTTP_GET, [this](AsyncWebServerRequest *request) { handleCaptivePortal(request); }); // Microsoft captive portal detection

    // OTA Routes
    _server.on("/check-update", HTTP_GET, [this](AsyncWebServerRequest *request) { handleCheckUpdate(request); });
    _server.on("/check-update", HTTP_OPTIONS, [this](AsyncWebServerRequest *request) { handleCorsPreflight(request); });
    _server.on("/start-update", HTTP_POST, [this](AsyncWebServerRequest *request) { 
        handleStartUpdate(request); 
    });
    _server.on("/start-update", HTTP_OPTIONS, [this](AsyncWebServerRequest *request) { handleCorsPreflight(request); });
    _server.on("/update-status", HTTP_GET, [this](AsyncWebServerRequest *request) { handleUpdateStatus(request); });
    _server.on("/update-status", HTTP_OPTIONS, [this](AsyncWebServerRequest *request) { handleCorsPreflight(request); });
    
    _server.on("/test-post", HTTP_POST, [this](AsyncWebServerRequest *request) { 
        Serial.println("CP DBG: Lambda for /test-post POST triggered."); 
        Serial.flush();
        request->send(200, "text/plain", "Test POST OK"); 
    });

    _server.onNotFound([this](AsyncWebServerRequest *request) { handle404(request); });
    
    _server.begin();
    Serial.println("Captive portal started with AsyncWebServer");
}

void CaptivePortal::handleRoot(AsyncWebServerRequest *request) {
    // Return the static HTML - network list will be populated by JavaScript
    request->send_P(200, "text/html", PORTAL_HTML);
}

void CaptivePortal::handleScanNetworks(AsyncWebServerRequest *request) {
    // Return cached networks immediately
    request->send(200, "application/json", _cachedNetworks);
    
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

void CaptivePortal::handleSaveWifi(AsyncWebServerRequest *request) {
    String ssid = "";
    String password = "";
    bool success = false;
    
    if (request->hasParam("ssid", true)) { // true for POST
        ssid = request->getParam("ssid", true)->value();
    }
    if (request->hasParam("password", true)) {
        password = request->getParam("password", true)->value();
    }
    
    if (ssid.length() > 0) {
        success = _configManager.saveWiFiCredentials(ssid, password);
        
        if (success) {
            _wifiInterface.stopAccessPoint();
            _wifiInterface.connectToStoredNetwork();
        }
    }
    
    String responseJson = "{\"success\":" + String(success ? "true" : "false") + "}";
    request->send(200, "application/json", responseJson);
}

void CaptivePortal::handleGetDeviceConfig(AsyncWebServerRequest *request) {
    StaticJsonDocument<256> doc;
    doc["teamId"] = _configManager.getTeamId();
    String truncatedKey = _configManager.getApiKey();
    if (truncatedKey.length() > 12) {
        truncatedKey = truncatedKey.substring(0, 12) + "...";
    }
    doc["apiKey"] = truncatedKey;
    
    String response;
    serializeJson(doc, response);
    request->send(200, "application/json", response);
}

void CaptivePortal::handleSaveDeviceConfig(AsyncWebServerRequest *request) {
    bool success = true;
    String message;
    
    if (request->hasParam("teamId", true)) { // true for POST
        int teamId = request->getParam("teamId", true)->value().toInt();
        _configManager.setTeamId(teamId);
    }
    
    if (request->hasParam("apiKey", true)) { // true for POST
        String apiKey = request->getParam("apiKey", true)->value();
        if (apiKey.endsWith("...")) {
            // success remains true, message indicates discarding
            message = "Discarding truncated API key";
        } else if (!_configManager.setApiKey(apiKey)) {
            success = false;
            message = "Invalid API key";
        }
    }
    
    StaticJsonDocument<128> doc;
    doc["success"] = success;
    if (!message.isEmpty()) { // Send message only if it's set
        doc["message"] = message;
    }
    
    String response;
    serializeJson(doc, response);
    request->send(200, "application/json", response);
}

void CaptivePortal::handleGetInsights(AsyncWebServerRequest *request) {
    DynamicJsonDocument doc(4096);
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
    request->send(200, "application/json", response);
}

void CaptivePortal::handleSaveInsight(AsyncWebServerRequest *request) {
    bool success = false;
    String message;
    
    if (request->hasParam("insightId", true)) { // true for POST
        String id = request->getParam("insightId", true)->value();
        
        success = _configManager.saveInsight(id, "");
        if (success) {
            _eventQueue.publishEvent(EventType::INSIGHT_ADDED, id);
        } else {
            message = "Failed to save insight";
        }
    } else {
        message = "Missing required fields";
    }
    
    StaticJsonDocument<128> doc;
    doc["success"] = success;
    if (!success) {
        doc["message"] = message;
    }
    
    String response;
    serializeJson(doc, response);
    request->send(200, "application/json", response);
}

void CaptivePortal::handleCaptivePortal(AsyncWebServerRequest *request) {
    request->redirect("http://" + _wifiInterface.getAPIPAddress());
}

void CaptivePortal::handle404(AsyncWebServerRequest *request) {
    request->redirect("http://" + _wifiInterface.getAPIPAddress());
}

String CaptivePortal::getNetworksJson() {
    DynamicJsonDocument doc(4096);
    JsonArray networks = doc.createNestedArray("networks");
    
    int numNetworks = WiFi.scanNetworks();
    
    if (numNetworks > 0) {
        int indices[numNetworks];
        for (int i = 0; i < numNetworks; i++) indices[i] = i;
        
        for (int i = 0; i < numNetworks; i++) {
            for (int j = i + 1; j < numNetworks; j++) {
                if (WiFi.RSSI(indices[j]) > WiFi.RSSI(indices[i])) {
                    std::swap(indices[i], indices[j]);
                }
            }
        }
        
        for (int i = 0; i < numNetworks; i++) {
            int index = indices[i];
            String ssid = WiFi.SSID(index);
            int rssi = WiFi.RSSI(index);
            bool encrypted = WiFi.encryptionType(index) != WIFI_AUTH_OPEN;
            
            if (ssid.length() > 0) {
                JsonObject network = networks.createNestedObject();
                network["ssid"] = ssid;
                network["rssi"] = rssi;
                network["encrypted"] = encrypted;
            }
        }
    }
    
    WiFi.scanDelete();
    
    String response;
    serializeJson(doc, response);
    return response;
}

void CaptivePortal::handleCorsPreflight(AsyncWebServerRequest *request) {
    Serial.println("CP DBG: handleCorsPreflight - Sending CORS preflight headers.");
    AsyncWebServerResponse *response = request->beginResponse(204);
    // Access-Control-Allow-Origin is handled by DefaultHeaders
    response->addHeader("Access-Control-Max-Age", "10000");
    response->addHeader("Access-Control-Allow-Methods", "POST,GET,OPTIONS");
    response->addHeader("Access-Control-Allow-Headers", "Origin, X-Requested-With, Content-Type, Accept");
    request->send(response);
}

void CaptivePortal::handleCheckUpdate(AsyncWebServerRequest *request) {
    Serial.println("CP DBG: handleCheckUpdate - Entered (Original Logic).");
    if (!_otaManager) {
        request->send(500, "application/json", "{\"error\":\"OTA manager not initialized\"}");
        return;
    }

    bool checkInitiated = _otaManager->checkForUpdate();
    UpdateStatus currentStatus = _otaManager->getStatus();
    String currentFirmware = _otaManager->getLastCheckResult().currentVersion; 
    if (currentFirmware.isEmpty()) {
        currentFirmware = CURRENT_FIRMWARE_VERSION;
    }

    DynamicJsonDocument doc(256);
    doc["check_initiated"] = checkInitiated;
    doc["current_firmware_version"] = currentFirmware;
    doc["initial_status_code"] = static_cast<int>(currentStatus.status);
    doc["initial_status_message"] = currentStatus.message;
    if (!checkInitiated && currentStatus.status == UpdateStatus::State::IDLE) {
        doc["initial_status_message"] = _otaManager->getLastCheckResult().error.isEmpty() ? currentStatus.message : _otaManager->getLastCheckResult().error;
    }

    String response;
    serializeJson(doc, response);
    Serial.printf("CP DBG: handleCheckUpdate - Sending response: %s\n", response.c_str());
    request->send(200, "application/json", response);
}

void CaptivePortal::handleStartUpdate(AsyncWebServerRequest *request) {
    Serial.println("CP DBG: handleStartUpdate - Entered (Original Logic).");
    if (!_otaManager) {
        Serial.println("CP DBG: handleStartUpdate - OTA manager not initialized.");
        request->send(500, "application/json", "{\"error\":\"OTA manager not initialized\"}");
        return;
    }

    String downloadUrl = _otaManager->getLastCheckResult().downloadUrl;
    Serial.printf("CP DBG: handleStartUpdate - URL from getLastCheckResult(): %s\n", downloadUrl.c_str());

    // Check if an update is actually available and a URL is present
    if (!_otaManager->getLastCheckResult().updateAvailable) {
        Serial.println("CP DBG: handleStartUpdate - getLastCheckResult() shows no update available. Aborting start.");
        request->send(400, "application/json", "{\"success\":false, \"message\":\"No update marked as available from the last check.\"}");
        return;
    }
    if (downloadUrl.isEmpty()) { // Should be redundant if updateAvailable is true and logic is correct in OtaManager
         Serial.println("CP DBG: handleStartUpdate - No download URL available from last check, though updateAvailable is true.");
         request->send(400, "application/json", "{\"success\":false, \"message\":\"No download URL available from last check (internal state).\"}");
        return;
    }

    Serial.println("CP DBG: handleStartUpdate - Calling _otaManager->beginUpdate().");
    bool success = _otaManager->beginUpdate(downloadUrl); 
    Serial.printf("CP DBG: handleStartUpdate - _otaManager->beginUpdate() returned: %s\n", success ? "true" : "false");
    
    StaticJsonDocument<128> doc;
    doc["success"] = success;
    if (!success) {
        // Get the latest status message if beginUpdate failed
        doc["message"] = _otaManager->getStatus().message; 
        Serial.printf("CP DBG: handleStartUpdate - Update start failed. Message: %s\n", _otaManager->getStatus().message.c_str());
    }
    
    String response;
    serializeJson(doc, response);
    Serial.printf("CP DBG: handleStartUpdate - Sending response: %s\n", response.c_str());
    request->send(200, "application/json", response);
}

void CaptivePortal::handleUpdateStatus(AsyncWebServerRequest *request) {
    Serial.println("CP DBG: handleUpdateStatus - Entered (Simplified Payload Test - Revived)."); // Modified log message
    if (!_otaManager) {
        Serial.println("CP DBG: handleUpdateStatus - OTA manager not initialized.");
        request->send(500, "application/json", "{\"error\":\"OTA manager not initialized\"}");
        return;
    }
    
    UpdateStatus status = _otaManager->getStatus();
    // UpdateInfo info = _otaManager->getLastCheckResult(); // KEEP THIS COMMENTED OUT
    
    DynamicJsonDocument doc(256); // Use smaller doc again
    doc["status_code"] = static_cast<int>(status.status);
    doc["status_message"] = status.message;
    doc["progress"] = status.progress;
    
    // Add a placeholder for is_update_available_info for the JS, default to false
    doc["is_update_available_info"] = false; 
    if (status.status == UpdateStatus::State::CHECKING_VERSION || // If check found update
        (status.status == UpdateStatus::State::IDLE && status.message.startsWith("Update available:"))) { // Or if idle but message indicates update
        doc["is_update_available_info"] = true;
    }

    // To keep JS somewhat functional, let's add placeholders for other fields it expects from UpdateInfo
    doc["current_firmware_version_info"] = CURRENT_FIRMWARE_VERSION; // Or a placeholder from status if available?
    doc["available_firmware_version_info"] = "N/A (Simple Status)"; 
    doc["release_notes_info"] = "N/A (Simple Status)";
    doc["error_message_info"] = ""; 
    if (static_cast<int>(status.status) >= static_cast<int>(UpdateStatus::State::ERROR_WIFI)) {
         doc["error_message_info"] = status.message; // Use status message as error if it's an error state
    }

    String response_str;
    serializeJson(doc, response_str);
    request->send(200, "application/json", response_str);
}