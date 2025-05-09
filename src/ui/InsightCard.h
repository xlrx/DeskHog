#pragma once

#include <lvgl.h>
#include <memory>
#include <functional>
#include <vector>
#include "ConfigManager.h"
#include "EventQueue.h"
#include "posthog/parsers/InsightParser.h"

/**
 * @class InsightCard
 * @brief UI component for displaying different data visualizations
 * 
 * Provides a flexible card-based UI component that can display various PostHog insights:
 * - Numeric displays (single value with formatted numbers)
 * - Line graphs (time series with auto-scaling)
 * - Funnel visualizations (with multi-breakdown support)
 * 
 * Features:
 * - Thread-safe UI updates via queue system
 * - Automatic insight type detection and UI adaptation
 * - Memory-safe LVGL object management
 * - Smart number formatting with unit scaling (K, M)
 */
class InsightCard {
public:
    /**
     * @brief Constructor
     * 
     * @param parent LVGL parent object to attach this card to
     * @param config Configuration manager for persistent storage
     * @param eventQueue Event queue for receiving data updates
     * @param insightId Unique identifier for this insight
     * @param width Card width in pixels
     * @param height Card height in pixels
     * 
     * Creates a card with a vertical flex layout containing:
     * - Title label with ellipsis for overflow
     * - Content container for visualization
     * Subscribes to INSIGHT_DATA_RECEIVED events for the specified insightId.
     */
    InsightCard(lv_obj_t* parent, ConfigManager& config, EventQueue& eventQueue,
                const String& insightId, uint16_t width, uint16_t height);
    
    /**
     * @brief Destructor - safely cleans up UI resources
     * 
     * Ensures all LVGL objects are deleted safely on the UI thread
     * using async deletion to prevent threading issues.
     */
    ~InsightCard();
    
    /**
     * @brief Get the underlying LVGL card object
     * 
     * @return LVGL object pointer for the main card container
     */
    lv_obj_t* getCard();

    /**
     * @brief Get the insight ID
     * 
     * @return String containing the unique insight identifier
     */
    String getInsightId() const;

    /**
     * @brief Initialize the UI update queue
     * 
     * Creates a FreeRTOS queue for handling UI updates across threads.
     * Must be called once before any InsightCards are created.
     */
    static void initUIQueue();

    /**
     * @brief Process pending UI updates
     * 
     * Processes all queued UI updates in the LVGL task context.
     * Should be called regularly from the LVGL handler task.
     * Forces immediate screen refresh after processing updates.
     */
    static void processUIQueue();
    
    /**
     * @brief Thread-safe method to dispatch UI updates to the LVGL task
     * 
     * @param update Lambda function containing UI operations
     * 
     * Queues UI operations to be executed on the LVGL thread.
     * Handles queue overflow by discarding updates if queue is full.
     */
    static void dispatchToLVGLTask(std::function<void()> update);

private:
    // Constants for UI layout and limits
    static constexpr int MAX_FUNNEL_STEPS = 5;     ///< Maximum number of steps in a funnel
    static constexpr int MAX_BREAKDOWNS = 5;       ///< Maximum number of breakdowns per step
    static constexpr int GRAPH_WIDTH = 240;        ///< Width of line graphs
    static constexpr int GRAPH_HEIGHT = 90;        ///< Height of line graphs
    static constexpr int FUNNEL_BAR_HEIGHT = 5;    ///< Height of each funnel bar
    static constexpr int FUNNEL_BAR_GAP = 20;      ///< Vertical gap between funnel bars
    static constexpr int FUNNEL_LEFT_MARGIN = 0;   ///< Left margin for funnel bars
    static constexpr int FUNNEL_LABEL_HEIGHT = 20; ///< Height of funnel step labels

    static QueueHandle_t uiQueue;  ///< Queue for thread-safe UI updates
    
    /**
     * @brief Thread-safe method to dispatch UI updates
     * 
     * @param update Lambda function containing UI operations
     * 
     * Queues UI operations to be executed on the LVGL thread.
     * Handles queue overflow by discarding updates if queue is full.
     */
    void dispatchToUI(std::function<void()> update);
    
    /**
     * @brief Handle events from the event queue
     * 
     * @param event Event containing insight data or JSON
     * 
     * Processes INSIGHT_DATA_RECEIVED events, parsing JSON if needed
     * and updating the visualization accordingly.
     */
    void onEvent(const Event& event);
    
    /**
     * @brief Process parsed insight data
     * 
     * @param parser Shared pointer to parsed insight data
     * 
     * Updates the card's visualization based on the insight type.
     * Handles type changes by recreating UI elements as needed.
     */
    void handleParsedData(std::shared_ptr<InsightParser> parser);
    
    /**
     * @brief Clear the content container
     * 
     * Safely removes all UI elements from the content container
     * and resets internal element pointers to nullptr.
     */
    void clearCardContent();
    
    /**
     * @brief Create UI elements for different visualization types
     * 
     * Each method creates the necessary LVGL objects for its visualization type:
     * - createNumericElements: Large centered value label
     * - createLineGraphElements: Chart with single data series
     * - createFunnelElements: Bars and labels for funnel steps with breakdown segments
     */
    void createNumericElements();
    void createLineGraphElements();
    void createFunnelElements();
    
    /**
     * @brief Update visualization with new data
     * 
     * Thread-safe methods to update each visualization type:
     * - updateNumericDisplay: Updates title and formatted value
     * - updateLineGraphDisplay: Updates chart with scaled data points
     * - updateFunnelDisplay: Updates funnel bars, labels, and breakdown segments
     */
    void updateNumericDisplay(const String& title, double value);
    void updateLineGraphDisplay(const String& title, double* values, size_t pointCount);
    void updateFunnelDisplay(const String& title, InsightParser& parser);
    
    /**
     * @brief Format a numeric value with appropriate scale and separators
     * 
     * @param value Value to format
     * @param buffer Buffer to store formatted string
     * @param bufferSize Size of buffer
     * 
     * Formats numbers with:
     * - M suffix for millions
     * - K suffix for thousands
     * - Thousands separators for integers
     * - 1-2 decimal places for fractional values
     */
    void formatNumber(double value, char* buffer, size_t bufferSize);
    
    /**
     * @brief Initialize funnel visualization colors
     * 
     * Sets up color palette for funnel breakdown segments:
     * - Blue, Green, Purple, Orange, Red
     */
    void initBreakdownColors();
    
    /**
     * @brief Check if an LVGL object is valid
     * 
     * @param obj LVGL object to check
     * @return true if object exists and is valid
     * 
     * Thread-safe method to verify LVGL object validity
     * before performing operations.
     */
    bool isValidObject(lv_obj_t* obj) const;
    
    // Configuration and state
    ConfigManager& _config;              ///< Configuration manager reference
    EventQueue& _event_queue;            ///< Event queue reference
    String _insight_id;                  ///< Unique insight identifier
    InsightParser::InsightType _current_type; ///< Current visualization type
    
    // UI Elements
    lv_obj_t* _card;                    ///< Main card container
    lv_obj_t* _title_label;             ///< Title text label
    lv_obj_t* _content_container;       ///< Container for visualization
    
    // Numeric display elements
    lv_obj_t* _value_label;             ///< Large value display label
    
    // Chart elements
    lv_obj_t* _chart;                   ///< Line graph chart
    lv_chart_series_t* _series;         ///< Data series for chart
    
    // Funnel elements
    lv_obj_t* _funnel_container;        ///< Container for funnel visualization
    lv_obj_t* _funnel_bars[MAX_FUNNEL_STEPS];          ///< Bar containers for each step
    lv_obj_t* _funnel_labels[MAX_FUNNEL_STEPS];        ///< Labels for each step
    lv_obj_t* _funnel_segments[MAX_FUNNEL_STEPS][MAX_BREAKDOWNS]; ///< Breakdown segments per step
    lv_color_t _breakdown_colors[MAX_BREAKDOWNS];       ///< Colors for breakdown segments
};