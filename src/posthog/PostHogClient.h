#ifndef POSTHOG_CLIENT_H
#define POSTHOG_CLIENT_H

#include <Arduino.h>
#include <HTTPClient.h>
#include <WiFi.h>
#include "ConfigManager.h"
#include "parsers/InsightParser.h"

class PostHogClient {
public:
    // Callback type for when data is fetched
    typedef void (*DataCallback)(void* context, const String& response);

    // Constructor
    PostHogClient(ConfigManager& configManager, const String& insightId);
    
    // Initialize the client and start periodic checking
    void begin();
    
    // Set callback for when data is fetched
    void onDataFetched(DataCallback callback, void* context);
    
    // Process function to be called periodically
    void process();
    
    // Check if client is ready (has API key and team ID)
    bool isReady() const;
    
private:
    // Config manager reference
    ConfigManager& _configManager;
    
    // Insight ID to fetch
    String _insight_id;
    
    // Base URL for PostHog API
    static const char* BASE_URL;
    
    // HTTP client instance
    HTTPClient _http;
    
    // Callback and context
    DataCallback _callback;
    void* _callback_context;
    
    // Last fetch attempt timestamp
    unsigned long _lastFetchAttempt;
    
    // Fetch interval in milliseconds
    static const unsigned long FETCH_INTERVAL = 5000;  // 5 seconds between attempts
    
    // Build full URL for insight
    String buildInsightUrl(const String& shortId) const;
    
    // Internal fetch method
    bool fetchInsight(const String& shortId, String& response);
};

#endif // POSTHOG_CLIENT_H 