#include "InsightCard.h"

#define GRAPH_WIDTH 180

InsightCard::InsightCard(lv_obj_t* parent, ConfigManager& config, const String& insightId,
                        uint16_t width, uint16_t height)
    : _config(config)
    , _insight_id(insightId)
    , _value_label(nullptr)
    , _chart(nullptr)
    , _series(nullptr)
    , _current_type(InsightParser::InsightType::INSIGHT_NOT_SUPPORTED) {
    
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
    
    // Create placeholder numeric display initially
    createNumericElements();
    
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
        updateNumericDisplay("Parse Error", 0);
        return;
    }
    
    auto insightType = parser.detectInsightType();
    char titleBuffer[64];
    
    if (!parser.getName(titleBuffer, sizeof(titleBuffer))) {
        strcpy(titleBuffer, "Unnamed Insight");
    }
    
    // If type has changed, rebuild UI elements
    if (insightType != _current_type) {
        _current_type = insightType;
        clearCardContent();
        
        if (insightType == InsightParser::InsightType::NUMERIC_CARD) {
            createNumericElements();
        } else if (insightType == InsightParser::InsightType::LINE_GRAPH) {
            createLineGraphElements();
        }
    }
    
    // Update based on type
    if (insightType == InsightParser::InsightType::NUMERIC_CARD) {
        double value = parser.getNumericCardValue();
        updateNumericDisplay(titleBuffer, value);
    } 
    else if (insightType == InsightParser::InsightType::LINE_GRAPH) {
        // Get y-values for the line graph
        size_t pointCount = parser.getSeriesPointCount();
        if (pointCount > 0) {
            double values[GRAPH_WIDTH];
            if (parser.getSeriesYValues(values)) {
                updateLineGraphDisplay(titleBuffer, values, pointCount);
            }
        }
    } 
    else {
        updateNumericDisplay("Unsupported Type", 0);
    }
}

void InsightCard::clearCardContent() {
    // Clear existing UI elements except the card and title
    if (_value_label) {
        lv_obj_del(_value_label);
        _value_label = nullptr;
    }
    
    if (_chart) {
        lv_obj_del(_chart);
        _chart = nullptr;
        _series = nullptr;  // Will be deleted with chart
    }
}

void InsightCard::createNumericElements() {
    // Create value label
    _value_label = lv_label_create(_card);
    lv_obj_align(_value_label, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_text_font(_value_label, &lv_font_montserrat_18, 0);
    lv_label_set_text(_value_label, "...");
}

void InsightCard::createLineGraphElements() {
    // Create chart
    _chart = lv_chart_create(_card);
    lv_obj_set_size(_chart, GRAPH_WIDTH, 90);
    lv_obj_align(_chart, LV_ALIGN_CENTER, 0, 5);
    lv_chart_set_type(_chart, LV_CHART_TYPE_LINE);
        
    // Add series with a vibrant color
    _series = lv_chart_add_series(_chart, lv_color_hex(0x2980b9), LV_CHART_AXIS_PRIMARY_Y);
    
    // Remove point markers and set line width
    lv_obj_set_style_size(_chart, 0, LV_PART_INDICATOR);
    lv_obj_set_style_line_width(_chart, 2, LV_PART_ITEMS);
    
    Serial.println("Line chart elements created");
}

void InsightCard::updateNumericDisplay(const String& title, double value) {
    lv_label_set_text(_title_label, title.c_str());
    
    // Format value with 2 decimal places if needed
    char valueBuffer[32];
    if (value == (int)value) {
        snprintf(valueBuffer, sizeof(valueBuffer), "%.0f", value);
    } else {
        snprintf(valueBuffer, sizeof(valueBuffer), "%.2f", value);
    }
    
    if (_value_label) {
        lv_label_set_text(_value_label, valueBuffer);
    }
}

void InsightCard::updateLineGraphDisplay(const String& title, double* values, size_t pointCount) {
    Serial.printf("\n=== Chart Update ===\n");
    Serial.printf("Point count: %d\n", pointCount);
    Serial.printf("Chart width: %d\n", GRAPH_WIDTH);
    
    // Set the title
    lv_label_set_text(_title_label, title.c_str());

    // Update point count
    lv_chart_set_point_count(_chart, pointCount);

    // Find max value for scaling
    double maxVal = values[0];
    for(size_t i = 0; i < pointCount; i++) {
        if(values[i] > maxVal) maxVal = values[i];
    }
    
    // Scale factor to keep values under 1000
    double scaleFactor = maxVal > 1000 ? 1000.0 / maxVal : 1.0;
    Serial.printf("Max value: %.2f, Scale factor: %.2f\n", maxVal, scaleFactor);

    // Print first and last few values to verify data
    Serial.println("First 3 values:");
    for(size_t i = 0; i < min(pointCount, (size_t)3); i++) {
        Serial.printf("  [%d]: %.2f -> %.2f\n", i, values[i], values[i] * scaleFactor);
    }
    
    Serial.println("Last 3 values:");
    for(size_t i = max((int)pointCount - 3, 0); i < pointCount; i++) {
        Serial.printf("  [%d]: %.2f -> %.2f\n", i, values[i], values[i] * scaleFactor);
    }

    for(size_t i = 0; i < pointCount; i++) {
        double scaledValue = values[i] * scaleFactor;
        lv_chart_set_next_value(_chart, _series, (int32_t)scaledValue);
    }

    lv_chart_refresh(_chart);
    Serial.println("Chart updated\n");
} 