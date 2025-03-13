#include "CaptivePortal.h"
#include <ArduinoJson.h>

CaptivePortal::CaptivePortal(ConfigManager& configManager, WiFiInterface& wifiInterface)
    : _server(80), _configManager(configManager), _wifiInterface(wifiInterface) {
}

void CaptivePortal::begin() {
    // Set up server routes
    _server.on("/", [this]() { handleRoot(); });
    _server.on("/scan-networks", [this]() { handleScanNetworks(); });
    _server.on("/save-wifi", [this]() { handleSaveWifi(); });
    _server.on("/get-device-config", [this]() { handleGetDeviceConfig(); });
    _server.on("/save-device-config", [this]() { handleSaveDeviceConfig(); });
    _server.on("/get-insights", [this]() { handleGetInsights(); });
    _server.on("/save-insight", [this]() { handleSaveInsight(); });
    _server.on("/delete-insight", [this]() { handleDeleteInsight(); });
    _server.on("/generate_204", [this]() { handleCaptivePortal(); }); // Android captive portal detection
    _server.on("/fwlink", [this]() { handleCaptivePortal(); }); // Microsoft captive portal detection
    
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
    // Return the main HTML with the networks list populated
    String html = FPSTR(PORTAL_HTML);
    html.replace("<!-- NETWORK_LIST_PLACEHOLDER -->", getScanNetworksHtml());
    _server.send(200, "text/html", html);
}

void CaptivePortal::handleScanNetworks() {
    // Just return the network list HTML for AJAX refresh
    _server.send(200, "text/html", getScanNetworksHtml());
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
    doc["apiKey"] = _configManager.getApiKey();
    
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
        if (!_configManager.setApiKey(apiKey)) {
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
    }
    
    String response;
    serializeJson(doc, response);
    _server.send(200, "application/json", response);
}

void CaptivePortal::handleSaveInsight() {
    bool success = false;
    String message;
    
    if (_server.hasArg("insightId") && _server.hasArg("insightContent")) {
        String id = _server.arg("insightId");
        String content = _server.arg("insightContent");
        
        success = _configManager.saveInsight(id, content);
        if (!success) {
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

String CaptivePortal::getScanNetworksHtml() {
    String html = "<option value=''>-- Select Network --</option>";
    
    // Scan for networks
    int numNetworks = WiFi.scanNetworks();
    
    if (numNetworks == 0) {
        html += "<option disabled>No networks found</option>";
    } else {
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
        
        // Add networks to dropdown
        for (int i = 0; i < numNetworks; i++) {
            int index = indices[i];
            String ssid = WiFi.SSID(index);
            int rssi = WiFi.RSSI(index);
            bool encrypted = WiFi.encryptionType(index) != WIFI_AUTH_OPEN;
            
            // Only show networks with an SSID
            if (ssid.length() > 0) {
                html += "<option value='" + ssid + "'>" + ssid;
                
                // Add signal strength indicator
                if (rssi >= -50) {
                    html += " (Excellent)";
                } else if (rssi >= -60) {
                    html += " (Good)";
                } else if (rssi >= -70) {
                    html += " (Fair)";
                } else {
                    html += " (Poor)";
                }
                
                // Add lock icon for encrypted networks
                if (encrypted) {
                    html += " 🔒";
                }
                
                html += "</option>";
            }
        }
    }
    
    // Free memory used by scan
    WiFi.scanDelete();
    
    return html;
}