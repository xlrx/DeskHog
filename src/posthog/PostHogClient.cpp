#include "PostHogClient.h"
#include "../ConfigManager.h"

const char* PostHogClient::BASE_URL = "https://us.posthog.com/api/projects/";

PostHogClient::PostHogClient(ConfigManager& config) 
    : _config(config)
    , has_active_request(false) {
    _http.setReuse(true);
}

void PostHogClient::subscribeToInsight(const String& insight_id, DataCallback callback, void* context) {
    InsightSubscription subscription = {
        .callback = callback,
        .callback_context = context,
        .last_refresh_time = 0
    };
    insight_subscriptions[insight_id] = subscription;
}

void PostHogClient::unsubscribeFromInsight(const String& insight_id) {
    insight_subscriptions.erase(insight_id);
}

void PostHogClient::queueRequest(const String& insight_id, DataCallback callback, void* context) {
    QueuedRequest request = {
        .insight_id = insight_id,
        .callback = callback,
        .callback_context = context
    };
    request_queue.push(request);
}

bool PostHogClient::isReady() const {
    return SystemController::isSystemFullyReady() && 
           _config.getTeamId() != ConfigManager::NO_TEAM_ID && 
           _config.getApiKey().length() > 0;
}

bool PostHogClient::hasSubscription(const String& insight_id) const {
    return insight_subscriptions.find(insight_id) != insight_subscriptions.end();
}

std::vector<String> PostHogClient::getSubscribedInsights() const {
    std::vector<String> insights;
    for (const auto& pair : insight_subscriptions) {
        insights.push_back(pair.first);
    }
    return insights;
}

void PostHogClient::process() {
    if (!isReady()) {
        return;
    }

    // Process any queued requests first
    if (!has_active_request) {
        processQueue();
    }

    // Check for needed refreshes
    if (!has_active_request) {
        checkRefreshes();
    }
}

void PostHogClient::onSystemStateChange(SystemState state) {
    if (!SystemController::isSystemFullyReady() == false) {
        // Clear any active request when system becomes not ready
        has_active_request = false;
    }
}

void PostHogClient::processQueue() {
    if (request_queue.empty()) {
        return;
    }

    QueuedRequest request = request_queue.front();
    String response;
    
    if (fetchInsight(request.insight_id, response)) {
        request.callback(request.callback_context, response);
        request_queue.pop();
    }
}

void PostHogClient::checkRefreshes() {
    unsigned long now = millis();
    
    for (auto& pair : insight_subscriptions) {
        const String& insight_id = pair.first;
        InsightSubscription& subscription = pair.second;
        
        if (now - subscription.last_refresh_time >= FETCH_INTERVAL) {
            String response;
            if (fetchInsight(insight_id, response)) {
                subscription.callback(subscription.callback_context, response);
                subscription.last_refresh_time = now;
            }
            break;  // Only process one refresh at a time
        }
    }
}

String PostHogClient::buildInsightUrl(const String& insight_id, const char* refresh_mode) const {
    String url = String(BASE_URL);
    url += String(_config.getTeamId());
    url += "/insights/?refresh=";
    url += refresh_mode;
    url += "&short_id=";
    url += insight_id;
    url += "&personal_api_key=";
    url += _config.getApiKey();
    return url;
}

bool PostHogClient::fetchInsight(const String& insight_id, String& response) {
    if (!isReady() || WiFi.status() != WL_CONNECTED) {
        return false;
    }

    has_active_request = true;
    
    // First, try to get cached data
    String url = buildInsightUrl(insight_id, "force_cache");
    
    _http.begin(url);
    int httpCode = _http.GET();
    
    bool success = false;
    bool needsRefresh = false;
    
    if (httpCode == HTTP_CODE_OK) {
        response = _http.getString();
        
        // Quick check if we need to refresh (look for null result)
        if (response.indexOf("\"result\":null") >= 0 || 
            response.indexOf("\"result\":[]") >= 0) {
            needsRefresh = true;
        } else {
            success = true;
        }
    }
    
    _http.end();
    
    // If we need to refresh, make a second request with blocking
    if (needsRefresh) {
        url = buildInsightUrl(insight_id, "blocking");
        
        _http.begin(url);
        httpCode = _http.GET();
        
        if (httpCode == HTTP_CODE_OK) {
            response = _http.getString();
            success = true;
        }
        
        _http.end();
    }
    
    has_active_request = false;
    return success;
} 