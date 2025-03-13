#include "PostHogClient.h"

const char* PostHogClient::BASE_URL = "https://us.posthog.com/api/projects/";

PostHogClient::PostHogClient(ConfigManager& configManager)
    : _configManager(configManager) {
}

void PostHogClient::begin() {
    _http.setReuse(true);
}

bool PostHogClient::isReady() const {
    return _configManager.getTeamId() != ConfigManager::NO_TEAM_ID && 
           _configManager.getApiKey().length() > 0;
}

String PostHogClient::buildInsightUrl(const String& shortId) const {
    String url = String(BASE_URL);
    url += String(_configManager.getTeamId());
    url += "/insights/?short_id=";
    url += shortId;
    url += "&personal_api_key=";
    url += _configManager.getApiKey();
    return url;
}

bool PostHogClient::fetchInsight(const String& shortId, String& response) {
    if (!isReady()) {
        return false;
    }
    
    String url = buildInsightUrl(shortId);
    
    _http.begin(url);
    int httpCode = _http.GET();
    
    bool success = false;
    if (httpCode == HTTP_CODE_OK) {
        response = _http.getString();
        success = true;
    }
    
    _http.end();
    return success;
} 