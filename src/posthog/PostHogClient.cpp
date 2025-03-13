#include "PostHogClient.h"

const char* PostHogClient::BASE_URL = "https://us.posthog.com/api/projects/";

PostHogClient::PostHogClient(ConfigManager& configManager, const String& insightId)
    : _configManager(configManager)
    , _insight_id(insightId)
    , _lastFetchAttempt(0)
    , _callback(nullptr)
    , _callback_context(nullptr) {
    Serial.printf("Creating PostHogClient for insight: %s\n", insightId.c_str());
    Serial.flush();
}

void PostHogClient::begin() {
    Serial.println("PostHogClient begin called");
    Serial.flush();
    _http.setReuse(true);
    _lastFetchAttempt = 0;  // Force first check on next process call
}

void PostHogClient::onDataFetched(DataCallback callback, void* context) {
    Serial.println("Setting callback");
    Serial.flush();
    _callback = callback;
    _callback_context = context;
}

void PostHogClient::process() {
    if (!isReady() || !_callback) {
        Serial.println("PostHogClient not ready or no callback set");
        Serial.flush();
        return;
    }

    unsigned long now = millis();
    if (now - _lastFetchAttempt >= FETCH_INTERVAL) {
        _lastFetchAttempt = now;
        
        if (WiFi.status() == WL_CONNECTED) {
            Serial.println("Network available, attempting fetch");
            Serial.flush();
            
            String response;
            if (fetchInsight(_insight_id, response)) {
                Serial.println("Fetch successful, calling callback");
                Serial.flush();
                _callback(_callback_context, response);
            } else {
                Serial.println("Fetch failed");
                Serial.flush();
            }
        } else {
            Serial.println("Network not available");
            Serial.flush();
        }
    }
}

bool PostHogClient::isReady() const {
    bool ready = _configManager.getTeamId() != ConfigManager::NO_TEAM_ID && 
                _configManager.getApiKey().length() > 0;
    if (!ready) {
        Serial.printf("Not ready - Team ID: %d, API Key length: %d\n", 
                     _configManager.getTeamId(), 
                     _configManager.getApiKey().length());
        Serial.flush();
    }
    return ready;
}

String PostHogClient::buildInsightUrl(const String& shortId) const {
    String url = String(BASE_URL);
    url += String(_configManager.getTeamId());
    url += "/insights/?short_id=";
    url += shortId;
    url += "&personal_api_key=";
    url += _configManager.getApiKey();
    Serial.printf("Built URL: %s\n", url.c_str());
    Serial.flush();
    return url;
}

bool PostHogClient::fetchInsight(const String& shortId, String& response) {
    String url = buildInsightUrl(shortId);
    
    _http.begin(url);
    int httpCode = _http.GET();
    
    Serial.printf("HTTP response code: %d\n", httpCode);
    Serial.flush();
    
    bool success = false;
    if (httpCode == HTTP_CODE_OK) {
        response = _http.getString();
        success = true;
        Serial.println("Got response body");
        Serial.flush();
    }
    
    _http.end();
    return success;
} 