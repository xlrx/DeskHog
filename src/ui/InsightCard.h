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
 * Supports numeric displays, line graphs, and funnel visualizations.
 * Thread-safe for receiving data from background tasks.
 */
class InsightCard {
public:
    /**
     * @brief Constructor
     * 
     * @param parent LVGL parent object
     * @param config Configuration manager reference
     * @param eventQueue Event queue for receiving data updates
     * @param insightId Unique identifier for this insight
     * @param width Card width
     * @param height Card height
     */
    InsightCard(lv_obj_t* parent, ConfigManager& config, EventQueue& eventQueue,
                const String& insightId, uint16_t width, uint16_t height);
    
    /**
     * @brief Destructor - safely cleans up UI resources
     */
    ~InsightCard();
    
    /**
     * @brief Get the underlying LVGL card object
     * 
     * @return LVGL object pointer
     */
    lv_obj_t* getCard();

    /**
     * @brief Get the insight ID
     * 
     * @return String Insight ID
     */
    String getInsightId() const;
    
private:
    // Constants
    static constexpr int MAX_FUNNEL_STEPS = 5;
    static constexpr int MAX_BREAKDOWNS = 5;
    static constexpr int GRAPH_WIDTH = 240;
    static constexpr int GRAPH_HEIGHT = 90;
    static constexpr int FUNNEL_BAR_HEIGHT = 5;
    static constexpr int FUNNEL_BAR_GAP = 20;
    static constexpr int FUNNEL_LEFT_MARGIN = 0;
    static constexpr int FUNNEL_LABEL_HEIGHT = 20;
    
    /**
     * @brief Thread-safe method to dispatch UI updates
     * 
     * @param update Function to execute on the UI thread
     */
    void dispatchToUI(std::function<void()> update);
    
    /**
     * @brief Handle events from the event queue
     */
    void onEvent(const Event& event);
    
    /**
     * @brief Process parsed insight data
     */
    void handleParsedData(std::shared_ptr<InsightParser> parser);
    
    /**
     * @brief Clear the content container
     */
    void clearCardContent();
    
    /**
     * @brief Create UI elements for different visualization types
     */
    void createNumericElements();
    void createLineGraphElements();
    void createFunnelElements();
    
    /**
     * @brief Update visualization with new data
     */
    void updateNumericDisplay(const String& title, double value);
    void updateLineGraphDisplay(const String& title, double* values, size_t pointCount);
    void updateFunnelDisplay(const String& title, InsightParser& parser);
    
    /**
     * @brief Format a numeric value with appropriate scale and separators
     */
    void formatNumber(double value, char* buffer, size_t bufferSize);
    
    /**
     * @brief Initialize funnel visualization colors
     */
    void initBreakdownColors();
    
    /**
     * @brief A safe wrapper to check if an LVGL object is valid
     * 
     * @param obj LVGL object to check
     * @return true if valid, false otherwise
     */
    bool isValidObject(lv_obj_t* obj) const;
    
    // Configuration
    ConfigManager& _config;
    EventQueue& _event_queue;
    String _insight_id;
    
    // UI Elements
    lv_obj_t* _card;
    lv_obj_t* _title_label;
    lv_obj_t* _content_container;
    
    // Numeric display elements
    lv_obj_t* _value_label;
    
    // Chart elements
    lv_obj_t* _chart;
    lv_chart_series_t* _series;
    
    // Funnel elements
    lv_obj_t* _funnel_container;
    lv_obj_t* _funnel_bars[MAX_FUNNEL_STEPS];
    lv_obj_t* _funnel_labels[MAX_FUNNEL_STEPS];
    lv_obj_t* _funnel_segments[MAX_FUNNEL_STEPS][MAX_BREAKDOWNS];
    lv_color_t _breakdown_colors[MAX_BREAKDOWNS];
    
    // State tracking
    InsightParser::InsightType _current_type;
};