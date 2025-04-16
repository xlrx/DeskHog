#pragma once

#include <Arduino.h>
#include <functional>
#include <vector>
#include <string>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>

/**
 * @brief Event types in the system
 */
enum class EventType {
    INSIGHT_ADDED,
    INSIGHT_DELETED,
    WIFI_CREDENTIALS_FOUND,
    NEED_WIFI_CREDENTIALS,
    WIFI_CONNECTING,
    WIFI_CONNECTED,
    WIFI_CONNECTION_FAILED,
    WIFI_AP_STARTED
};

/**
 * @brief Represents an event in the system
 */
struct Event {
    EventType type;     // Type of event
    String insightId;   // ID of the insight related to the event
    
    Event() {}
    
    Event(EventType t, const String& id) : type(t), insightId(id) {}
};

/**
 * @brief Callback function type for event handlers
 */
using EventCallback = std::function<void(const Event&)>;

/**
 * @brief Thread-safe event queue for handling system events
 */
class EventQueue {
private:
    QueueHandle_t eventQueue;
    SemaphoreHandle_t callbackMutex;
    std::vector<EventCallback> eventCallbacks;
    
    static void eventProcessingTask(void* parameter);
    TaskHandle_t taskHandle;
    bool isRunning;
    
public:
    EventQueue(size_t queueSize = 10);
    ~EventQueue();
    
    /**
     * @brief Publish an event to the queue
     * 
     * @param eventType Type of the event
     * @param insightId ID of the insight related to the event
     * @return true if the event was successfully queued
     * @return false if the queue is full
     */
    bool publishEvent(EventType eventType, const String& insightId);
    
    /**
     * @brief Alternative method to publish a pre-constructed Event
     * 
     * @param event The event to publish
     * @return true if the event was successfully queued
     * @return false if the queue is full
     */
    bool publishEvent(const Event& event);
    
    /**
     * @brief Subscribe to events
     * 
     * @param callback Function to call when an event is processed
     */
    void subscribe(EventCallback callback);
    
    /**
     * @brief Start the event processing task
     */
    void begin();
    
    /**
     * @brief Stop the event processing task
     */
    void end();
}; 