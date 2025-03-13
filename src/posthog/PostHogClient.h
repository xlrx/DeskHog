#ifndef POSTHOG_CLIENT_H
#define POSTHOG_CLIENT_H

#include <Arduino.h>
#include <HTTPClient.h>
#include "ConfigManager.h"
#include "parsers/InsightParser.h"

class PostHogClient {
public:
    // Constructor
    PostHogClient(ConfigManager& configManager);
    
    // Initialize the client
    void begin();
    
    // Fetch insight data by short ID
    bool fetchInsight(const String& shortId, String& response);
    
    // Check if client is ready (has API key and team ID)
    bool isReady() const;
    
private:
    // Config manager reference
    ConfigManager& _configManager;
    
    // Base URL for PostHog API
    static const char* BASE_URL;
    
    // HTTP client instance
    HTTPClient _http;
    
    // Build full URL for insight
    String buildInsightUrl(const String& shortId) const;
};

#endif // POSTHOG_CLIENT_H 