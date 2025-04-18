#pragma once

#include <Arduino.h>
#include <HTTPClient.h>
#include <WiFi.h>
#include <queue>
#include <vector>
#include <set>
#include <memory>
#include "../ConfigManager.h"
#include "SystemController.h"
#include "EventQueue.h"
#include "parsers/InsightParser.h"

class PostHogClient {
public:
    // Constructor
    explicit PostHogClient(ConfigManager& config, EventQueue& eventQueue);
    
    // Delete copy constructor and assignment operator
    PostHogClient(const PostHogClient&) = delete;
    void operator=(const PostHogClient&) = delete;
    
    // Queue an immediate fetch (separate from refresh cycle)
    void requestInsightData(const String& insight_id);
    
    // Utility methods
    bool isReady() const;
    
    // Process function to be called in loop
    void process();
    
private:
    // Configuration
    ConfigManager& _config;
    EventQueue& _eventQueue;
    
    // Queue request tracking
    struct QueuedRequest {
        String insight_id;
        uint8_t retry_count;
    };
    
    // Member variables
    std::set<String> requested_insights;  // Track all insights we've seen
    std::queue<QueuedRequest> request_queue;
    bool has_active_request;
    HTTPClient _http;
    unsigned long last_refresh_check;  // When we last checked for refreshes
    
    // Constants
    static const char* BASE_URL;
    static const unsigned long REFRESH_INTERVAL = 30000; // 30 seconds
    static const uint8_t MAX_RETRIES = 3;              // Maximum number of retry attempts
    static const unsigned long RETRY_DELAY = 1000;     // Delay between retries (ms)
    
    // Private methods
    void onSystemStateChange(SystemState state);
    void processQueue();
    void checkRefreshes();
    bool fetchInsight(const String& insight_id, String& response);
    String buildInsightUrl(const String& insight_id, const char* refresh_mode = "force_cache") const;
    
    // Event-related methods
    void publishInsightDataEvent(const String& insight_id, const String& response);
}; 