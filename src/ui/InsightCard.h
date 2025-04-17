#pragma once

#include <Arduino.h>
#include <lvgl.h>
#include <functional>
#include <memory>
#include "lv_conf.h"
#include "../ConfigManager.h"
#include "../posthog/parsers/InsightParser.h"
#include "../EventQueue.h"

class InsightCard {
public:
    InsightCard(lv_obj_t* parent, ConfigManager& config, EventQueue& eventQueue, 
                const String& insightId, uint16_t width, uint16_t height);
    ~InsightCard();
    
    lv_obj_t* getCard();
    
    // Get the insight ID
    String getInsightId() const { return _insight_id; }

protected:
    // Thread-safe UI dispatch helper
    void dispatchToUI(std::function<void()> update);
    
    // Protected UI update methods - all guaranteed to run on main thread
    void createNumericElements();
    void createLineGraphElements();
    void createFunnelElements();
    void clearCardContent();
    
    // Data handling methods
    void handleParsedData(std::shared_ptr<InsightParser> parser);
    void onEvent(const Event& event);

private:
    static constexpr size_t MAX_FUNNEL_STEPS = 5;
    static constexpr size_t MAX_BREAKDOWNS = 5;
    
    ConfigManager& _config;
    EventQueue& _event_queue;
    String _insight_id;
    
    // LVGL Objects
    lv_obj_t* _card;
    lv_obj_t* _title_label;
    lv_obj_t* _value_label;
    lv_obj_t* _chart;
    lv_chart_series_t* _series;
    lv_obj_t* _funnel_container;
    lv_obj_t* _content_container;  // Container for the main content (numeric, chart, or funnel)
    lv_obj_t* _funnel_bars[MAX_FUNNEL_STEPS];
    lv_obj_t* _funnel_labels[MAX_FUNNEL_STEPS];
    lv_obj_t* _funnel_segments[MAX_FUNNEL_STEPS][MAX_BREAKDOWNS];
    
    // Visualization state
    InsightParser::InsightType _current_type;
    lv_color_t _breakdown_colors[MAX_BREAKDOWNS];
    
    // UI update methods - all called through dispatchToUI
    void updateNumericDisplay(const String& title, double value);
    void updateLineGraphDisplay(const String& title, double* values, size_t pointCount);
    void updateFunnelDisplay(const String& title, InsightParser& parser);
    void initBreakdownColors();
}; 