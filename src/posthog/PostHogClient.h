#ifndef POSTHOG_CLIENT_H
#define POSTHOG_CLIENT_H

#include <Arduino.h>
#include <HTTPClient.h>
#include <WiFi.h>
#include <map>
#include <queue>
#include <vector>
#include "../ConfigManager.h"
#include "SystemController.h"

class PostHogClient {
public:
    // Callback type for when data is fetched
    typedef void (*DataCallback)(void* context, const String& response);
    
    // Constructor
    explicit PostHogClient(ConfigManager& config);
    
    // Delete copy constructor and assignment operator
    PostHogClient(const PostHogClient&) = delete;
    void operator=(const PostHogClient&) = delete;
    
    // Subscription management
    void subscribeToInsight(const String& insight_id, DataCallback callback, void* context);
    void unsubscribeFromInsight(const String& insight_id);
    
    // Queue an immediate fetch (separate from refresh cycle)
    void queueRequest(const String& insight_id, DataCallback callback, void* context);
    
    // Utility methods
    bool isReady() const;
    bool hasSubscription(const String& insight_id) const;
    std::vector<String> getSubscribedInsights() const;
    
    // Process function to be called in loop
    void process();
    
private:
    // Configuration
    ConfigManager& _config;
    
    // Subscription tracking
    struct InsightSubscription {
        DataCallback callback;
        void* callback_context;
        unsigned long last_refresh_time;
    };
    
    // Queue request tracking
    struct QueuedRequest {
        String insight_id;
        DataCallback callback;
        void* callback_context;
    };
    
    // Member variables
    std::map<String, InsightSubscription> insight_subscriptions;
    std::queue<QueuedRequest> request_queue;
    bool has_active_request;
    HTTPClient _http;
    
    // Constants
    static const char* BASE_URL;
    static const unsigned long FETCH_INTERVAL = 5000;  // 5 seconds between refreshes
    
    // Private methods
    void onSystemStateChange(SystemState state);
    void processQueue();
    void checkRefreshes();
    bool fetchInsight(const String& insight_id, String& response);
    String buildInsightUrl(const String& insight_id, const char* refresh_mode = "force_cache") const;
};

#endif // POSTHOG_CLIENT_H 