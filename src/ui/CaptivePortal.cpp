#include "CaptivePortal.h"

CaptivePortal::CaptivePortal(ConfigManager& configManager, WiFiInterface& wifiInterface)
    : _server(80), _configManager(configManager), _wifiInterface(wifiInterface) {
}

void CaptivePortal::begin() {
    // Set up server routes
    _server.on("/", [this]() { handleRoot(); });
    _server.on("/wifi", [this]() { handleWifiConfig(); });
    _server.on("/generate_204", [this]() { handleCaptivePortal(); });
    _server.on("/fwlink", [this]() { handleCaptivePortal(); });
    
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
    String html = "";
    
    sendHeader("ESP32 WiFi Setup");
    
    html += "<div class='container'>";
    html += "<h2>Welcome to ESP32 WiFi Setup</h2>";
    html += "<p>This portal allows you to configure your device's WiFi connection.</p>";
    html += "<p>Click the button below to scan for available networks and set up your WiFi connection.</p>";
    html += "<div class='button-container'>";
    html += "<a href='/wifi' class='button'>Configure WiFi</a>";
    html += "</div>";
    html += "</div>";
    
    sendFooter();
    
    _server.send(200, "text/html", html);
}

void CaptivePortal::handleWifiConfig() {
    if (_server.method() == HTTP_POST) {
        String ssid = _server.arg("ssid");
        String password = _server.arg("password");
        
        if (ssid.length() > 0) {
            String html = "";
            
            sendHeader("WiFi Configuration");
            
            html += "<div class='container'>";
            
            if (_configManager.saveWiFiCredentials(ssid, password)) {
                html += "<h2>WiFi Configuration Saved</h2>";
                html += "<p>The device will now attempt to connect to the WiFi network.</p>";
                html += "<p>If the connection is successful, the device will restart in normal mode.</p>";
                html += "<p>If the connection fails, the setup portal will be available again.</p>";
                html += getRedirectScript("/", 10);
            } else {
                html += "<h2>Error Saving Configuration</h2>";
                html += "<p>There was an error saving your WiFi configuration.</p>";
                html += "<p>Please try again.</p>";
                html += "<div class='button-container'>";
                html += "<a href='/wifi' class='button'>Back</a>";
                html += "</div>";
            }
            
            html += "</div>";
            
            sendFooter();
            
            _server.send(200, "text/html", html);
            
            // Try to connect to the new network
            _wifiInterface.stopAccessPoint();
            _wifiInterface.connectToStoredNetwork();
            
            return;
        }
    }
    
    // GET request or invalid POST
    String html = "";
    
    sendHeader("WiFi Configuration");
    
    html += "<div class='container'>";
    html += "<h2>Configure WiFi Connection</h2>";
    html += "<p>Please select your WiFi network and enter the password if required.</p>";
    
    html += "<form method='post' action='/wifi'>";
    html += "<div class='form-group'>";
    html += "<label for='ssid'>WiFi Network:</label>";
    html += "<select name='ssid' id='ssid' required>";
    
    // Add network scan results
    html += getScanNetworksHtml();
    
    html += "</select>";
    html += "</div>";
    
    html += "<div class='form-group'>";
    html += "<label for='password'>Password:</label>";
    html += "<input type='password' name='password' id='password'>";
    html += "</div>";
    
    html += "<div class='button-container'>";
    html += "<button type='submit' class='button'>Save Configuration</button>";
    html += "</div>";
    html += "</form>";
    
    html += "<div class='button-container' style='margin-top: 20px;'>";
    html += "<a href='/wifi' class='button secondary'>Refresh Network List</a>";
    html += "</div>";
    
    html += "</div>";
    
    sendFooter();
    
    _server.send(200, "text/html", html);
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

void CaptivePortal::sendHeader(const String& title) {
    String html = "<!DOCTYPE html>";
    html += "<html>";
    html += "<head>";
    html += "<meta charset='UTF-8'>";
    html += "<meta name='viewport' content='width=device-width, initial-scale=1.0'>";
    html += "<title>" + title + "</title>";
    html += "<style>";
    html += "body { font-family: Arial, sans-serif; margin: 0; padding: 0; background-color: #f5f5f5; color: #333; }";
    html += ".container { max-width: 800px; margin: 20px auto; padding: 20px; background-color: white; border-radius: 5px; box-shadow: 0 2px 5px rgba(0,0,0,0.1); }";
    html += "h1, h2 { color: #0066cc; }";
    html += ".header { background-color: #0066cc; color: white; padding: 10px 20px; text-align: center; }";
    html += ".form-group { margin-bottom: 15px; }";
    html += "label { display: block; margin-bottom: 5px; font-weight: bold; }";
    html += "input[type='text'], input[type='password'], select { width: 100%; padding: 8px; border: 1px solid #ddd; border-radius: 4px; box-sizing: border-box; }";
    html += ".button-container { margin-top: 15px; text-align: center; }";
    html += ".button { display: inline-block; padding: 10px 20px; background-color: #0066cc; color: white; text-decoration: none; border-radius: 4px; border: none; cursor: pointer; }";
    html += ".button.secondary { background-color: #777; }";
    html += ".button:hover { background-color: #0055aa; }";
    html += ".button.secondary:hover { background-color: #666; }";
    html += "footer { text-align: center; margin-top: 20px; color: #777; font-size: 0.8em; }";
    html += "</style>";
    html += "</head>";
    html += "<body>";
    html += "<div class='header'>";
    html += "<h1>" + title + "</h1>";
    html += "</div>";
    
    _server.sendContent(html);
}

void CaptivePortal::sendFooter() {
    String html = "<footer>&copy; " + String(2024) + " ESP32 WiFi Setup</footer>";
    html += "</body>";
    html += "</html>";
    
    _server.sendContent(html);
}

String CaptivePortal::getRedirectScript(const String& url, int delay) {
    String html = "<script>";
    html += "setTimeout(function() {";
    html += "window.location.href = '" + url + "';";
    html += "}, " + String(delay * 1000) + ");";
    html += "</script>";
    html += "<p>Redirecting in " + String(delay) + " seconds...</p>";
    
    return html;
}