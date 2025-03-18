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
    void process();  // Call this periodically
    
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
    
    String _insight_id;
    ConfigManager& _config;
    PostHogClient* _client;  // Owned by this card
    InsightParser::InsightType _current_type;
    
    void updateNumericDisplay(const String& title, double value);
    void updateLineGraphDisplay(const String& title, double* values, size_t pointCount);
    void handleNewData(const String& response);
    void createNumericElements();
    void createLineGraphElements();
    void clearCardContent();
};

#endif // INSIGHT_CARD_H 