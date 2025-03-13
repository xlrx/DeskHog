#include "InsightCard.h"

InsightCard::InsightCard(lv_obj_t* parent, ConfigManager& config, const String& insightId,
                        uint16_t width, uint16_t height)
    : _config(config)
    , _insight_id(insightId) {
    
    Serial.printf("\n\n=== Creating InsightCard for insight ID: %s ===\n", insightId.c_str());
    Serial.printf("Team ID: %d\n", _config.getTeamId());
    Serial.printf("API Key length: %d\n", _config.getApiKey().length());
    Serial.flush();
    
    // Create PostHog client
    _client = new PostHogClient(config, insightId);
    _client->onDataFetched(onDataReceived, this);
    Serial.println("PostHog client created");
    Serial.flush();
    
    _client->begin();
    Serial.println("PostHog client initialized");
    Serial.flush();
    
    // Create card
    _card = lv_obj_create(parent);
    lv_obj_set_size(_card, width, height);
    lv_obj_clear_flag(_card, LV_OBJ_FLAG_SCROLLABLE);
    
    // Create title label
    _title_label = lv_label_create(_card);
    lv_obj_align(_title_label, LV_ALIGN_TOP_MID, 0, 10);
    lv_label_set_text(_title_label, "Loading...");
    
    // Create value label
    _value_label = lv_label_create(_card);
    lv_obj_align(_value_label, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_text_font(_value_label, &lv_font_montserrat_18, 0);
    lv_label_set_text(_value_label, "...");
    
    Serial.println("Card UI elements created");
    Serial.flush();
    
    // Initial data fetch
    process();
    
    Serial.println("Initial process call complete");
    Serial.flush();
}

InsightCard::~InsightCard() {
    delete _client;
}

lv_obj_t* InsightCard::getCard() {
    return _card;
}

void InsightCard::process() {
    _client->process();
}

void InsightCard::onDataReceived(void* context, const String& response) {
    InsightCard* card = static_cast<InsightCard*>(context);
    card->handleNewData(response);
}

void InsightCard::handleNewData(const String& response) {
    InsightParser parser(response.c_str());
    
    if (!parser.isValid()) {
        updateDisplay("Parse Error", 0);
        return;
    }
    
    auto insightType = parser.detectInsightType();
    if (insightType == InsightParser::InsightType::NUMERIC_CARD) {
        char titleBuffer[64];
        if (parser.getName(titleBuffer, sizeof(titleBuffer))) {
            double value = parser.getNumericCardValue();
            updateDisplay(titleBuffer, value);
        } else {
            updateDisplay("Name Error", 0);
        }
    } else {
        updateDisplay("Unsupported Type", 0);
    }
}

void InsightCard::updateDisplay(const String& title, double value) {
    Serial.printf("Updating display - Title: %s, Value: %f\n", title.c_str(), value);
    
    lv_label_set_text(_title_label, title.c_str());
    
    // Format value with 2 decimal places if needed
    char valueBuffer[32];
    if (value == (int)value) {
        snprintf(valueBuffer, sizeof(valueBuffer), "%.0f", value);
    } else {
        snprintf(valueBuffer, sizeof(valueBuffer), "%.2f", value);
    }
    lv_label_set_text(_value_label, valueBuffer);
    
    Serial.printf("Display updated with formatted value: %s\n", valueBuffer);
} 