#ifndef INSIGHT_CARD_H
#define INSIGHT_CARD_H

#include <Arduino.h>
#include <lvgl.h>
#include "../ConfigManager.h"
#include "../posthog/PostHogClient.h"
#include "../posthog/parsers/InsightParser.h"

class InsightCard {
public:
    InsightCard(lv_obj_t* parent, ConfigManager& config, const String& insightId, 
                uint16_t width, uint16_t height);
    ~InsightCard();
    
    lv_obj_t* getCard();
    
    // Static callback for PostHogClient
    static void onDataReceived(void* context, const String& response);
    
private:
    lv_obj_t* _card;
    lv_obj_t* _title_label;
    
    // Elements for numeric card
    lv_obj_t* _value_label;
    
    // Elements for line graph
    lv_obj_t* _chart;
    lv_chart_series_t* _series;

    // Elements for funnel visualization
    static const uint8_t MAX_FUNNEL_STEPS = 10;
    static const uint8_t MAX_BREAKDOWNS = 5;
    
    lv_obj_t* _funnel_container;  // Container for all funnel elements
    lv_obj_t* _funnel_bars[MAX_FUNNEL_STEPS];  // Container for each step
    lv_obj_t* _funnel_segments[MAX_FUNNEL_STEPS][MAX_BREAKDOWNS];  // Breakdown segments within each step
    lv_obj_t* _funnel_labels[MAX_FUNNEL_STEPS];  // Labels for counts/conversion rates
    lv_color_t _breakdown_colors[MAX_BREAKDOWNS];  // Colors for each breakdown
    
    String _insight_id;
    ConfigManager& _config;
    InsightParser::InsightType _current_type;
    
    void updateNumericDisplay(const String& title, double value);
    void updateLineGraphDisplay(const String& title, double* values, size_t pointCount);
    void handleNewData(const String& response);
    void createNumericElements();
    void createLineGraphElements();
    void createFunnelElements();
    void updateFunnelDisplay(const String& title, InsightParser& parser);
    void clearCardContent();
    void initBreakdownColors();  // Initialize colors for funnel breakdowns
};

#endif // INSIGHT_CARD_H 