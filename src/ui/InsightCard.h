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
    lv_obj_t* _value_label;
    String _insight_id;
    ConfigManager& _config;
    PostHogClient* _client;  // Owned by this card
    
    void updateDisplay(const String& title, double value);
    void handleNewData(const String& response);
};

#endif // INSIGHT_CARD_H 