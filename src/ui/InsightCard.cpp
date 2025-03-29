#include "InsightCard.h"

#define GRAPH_WIDTH 240  // Full screen width
#define FUNNEL_BAR_HEIGHT 15
#define FUNNEL_BAR_GAP 20    // Increased to accommodate label
#define FUNNEL_LEFT_MARGIN 0  // No left margin
#define FUNNEL_LABEL_HEIGHT 20

InsightCard::InsightCard(lv_obj_t* parent, ConfigManager& config, const String& insightId,
                        uint16_t width, uint16_t height)
    : _config(config)
    , _insight_id(insightId)
    , _value_label(nullptr)
    , _chart(nullptr)
    , _series(nullptr)
    , _current_type(InsightParser::InsightType::INSIGHT_NOT_SUPPORTED) {
    
    // Initialize funnel arrays to nullptr
    for (int i = 0; i < MAX_FUNNEL_STEPS; i++) {
        _funnel_bars[i] = nullptr;
        _funnel_labels[i] = nullptr;
        for (int j = 0; j < MAX_BREAKDOWNS; j++) {
            _funnel_segments[i][j] = nullptr;
        }
    }
    
    // Initialize breakdown colors
    initBreakdownColors();
    
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
        } else if (insightType == InsightParser::InsightType::FUNNEL) {
            createFunnelElements();
        }
    }
    
    // Update based on type
    if (insightType == InsightParser::InsightType::NUMERIC_CARD) {
        double value = parser.getNumericCardValue();
        updateNumericDisplay(titleBuffer, value);
    } 
    else if (insightType == InsightParser::InsightType::LINE_GRAPH) {
        size_t pointCount = parser.getSeriesPointCount();
        if (pointCount > 0) {
            double values[GRAPH_WIDTH];
            if (parser.getSeriesYValues(values)) {
                updateLineGraphDisplay(titleBuffer, values, pointCount);
            }
        }
    }
    else if (insightType == InsightParser::InsightType::FUNNEL) {
        updateFunnelDisplay(titleBuffer, parser);
    }
    else {
        updateNumericDisplay("Unsupported Type", 0);
    }
}

void InsightCard::clearCardContent() {
    // First clear all our references to child objects
    for (int i = 0; i < MAX_FUNNEL_STEPS; i++) {
        for (int j = 0; j < MAX_BREAKDOWNS; j++) {
            _funnel_segments[i][j] = nullptr;
        }
        _funnel_bars[i] = nullptr;
        _funnel_labels[i] = nullptr;
    }
    
    // Now safely delete the containers
    if (_value_label) {
        lv_obj_del_async(_value_label);
        _value_label = nullptr;
    }
    
    if (_chart) {
        lv_obj_del_async(_chart);
        _chart = nullptr;
        _series = nullptr;  // Will be deleted with chart
    }
    
    if (_funnel_container) {
        lv_obj_del_async(_funnel_container);
        _funnel_container = nullptr;
    }
    
    if (_title_label) {
        lv_obj_del_async(_title_label);
    }
    
    // Recreate title label after a short delay
    lv_timer_t* timer = lv_timer_create([](lv_timer_t* timer) {
        InsightCard* card = (InsightCard*)timer->user_data;
        card->_title_label = lv_label_create(card->_card);
        lv_obj_align(card->_title_label, LV_ALIGN_BOTTOM_MID, 0, -2);
        lv_label_set_text(card->_title_label, "Loading...");
        lv_timer_del(timer);
    }, 50, this);
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

void InsightCard::initBreakdownColors() {
    // Initialize with distinct colors
    _breakdown_colors[0] = lv_color_hex(0x2980b9);  // Blue
    _breakdown_colors[1] = lv_color_hex(0x27ae60);  // Green
    _breakdown_colors[2] = lv_color_hex(0x8e44ad);  // Purple
    _breakdown_colors[3] = lv_color_hex(0xd35400);  // Orange
    _breakdown_colors[4] = lv_color_hex(0xc0392b);  // Red
}

void InsightCard::createFunnelElements() {
    // Create container for funnel visualization
    _funnel_container = lv_obj_create(_card);
    lv_obj_set_size(_funnel_container, GRAPH_WIDTH, 90);
    lv_obj_align(_funnel_container, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_clear_flag(_funnel_container, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_pad_all(_funnel_container, 0, 0);
    lv_obj_set_style_border_width(_funnel_container, 0, 0);
    lv_obj_set_style_bg_opa(_funnel_container, LV_OPA_0, 0);
}

void InsightCard::updateFunnelDisplay(const String& title, InsightParser& parser) {
    // Update title at bottom
    lv_label_set_text(_title_label, title.c_str());
    
    size_t stepCount = parser.getFunnelStepCount();
    size_t breakdownCount = parser.getFunnelBreakdownCount();
    
    if (stepCount == 0 || breakdownCount == 0) {
        return;
    }
    
    // Limit to maximum supported steps and breakdowns
    stepCount = min(stepCount, (size_t)MAX_FUNNEL_STEPS);
    breakdownCount = min(breakdownCount, (size_t)MAX_BREAKDOWNS);
    
    // Get first step data to calculate totals
    uint32_t breakdownCounts[MAX_BREAKDOWNS];
    double breakdownRates[MAX_BREAKDOWNS];
    parser.getFunnelBreakdownComparison(0, breakdownCounts, breakdownRates);
    
    uint32_t totalFirstStep = 0;
    for (size_t i = 0; i < breakdownCount; i++) {
        totalFirstStep += breakdownCounts[i];
    }
    
    // Start from top of screen
    int yOffset = 0;
    
    // Create or update bars for each step
    for (size_t step = 0; step < stepCount; step++) {
        // Create step container if needed
        if (!_funnel_bars[step]) {
            _funnel_bars[step] = lv_obj_create(_funnel_container);
            lv_obj_set_size(_funnel_bars[step], GRAPH_WIDTH, FUNNEL_BAR_HEIGHT);
            lv_obj_set_style_bg_opa(_funnel_bars[step], LV_OPA_0, 0);
            lv_obj_set_style_border_width(_funnel_bars[step], 0, 0);
            lv_obj_set_style_pad_all(_funnel_bars[step], 0, 0);
            lv_obj_clear_flag(_funnel_bars[step], LV_OBJ_FLAG_SCROLLABLE);
        }
        
        // Position the bar with no margin
        lv_obj_align(_funnel_bars[step], LV_ALIGN_TOP_LEFT, 0, yOffset);
        
        // Create or update label
        if (!_funnel_labels[step]) {
            _funnel_labels[step] = lv_label_create(_funnel_container);
            lv_obj_set_style_text_align(_funnel_labels[step], LV_TEXT_ALIGN_LEFT, 0);
        }
        
        // Get breakdown data for this step
        parser.getFunnelBreakdownComparison(step, breakdownCounts, breakdownRates);
        
        // Calculate total for this step
        uint32_t stepTotal = 0;
        for (size_t i = 0; i < breakdownCount; i++) {
            stepTotal += breakdownCounts[i];
        }
        
        // Format and set label text
        char labelBuffer[32];
        if (step == 0) {
            snprintf(labelBuffer, sizeof(labelBuffer), "%d", stepTotal);
        } else {
            snprintf(labelBuffer, sizeof(labelBuffer), "%d (%d%%)", 
                    stepTotal,
                    totalFirstStep > 0 ? (stepTotal * 100) / totalFirstStep : 0);
        }
        lv_label_set_text(_funnel_labels[step], labelBuffer);
        
        // Position label below the bar with minimal left margin
        lv_obj_align(_funnel_labels[step], LV_ALIGN_TOP_LEFT, 1, yOffset + FUNNEL_BAR_HEIGHT + 2);
        
        // Calculate this step's total width based on first step total
        float stepPercentage = (float)stepTotal / totalFirstStep;
        float totalWidth = GRAPH_WIDTH * stepPercentage;
        
        // Create or update breakdown segments
        float currentWidth = 0;
        for (size_t breakdown = 0; breakdown < breakdownCount; breakdown++) {
            float segmentPercentage = (float)breakdownCounts[breakdown] / stepTotal;
            float segmentWidth = totalWidth * segmentPercentage;
            
            if (!_funnel_segments[step][breakdown]) {
                _funnel_segments[step][breakdown] = lv_obj_create(_funnel_bars[step]);
                lv_obj_set_style_bg_color(_funnel_segments[step][breakdown], 
                                        _breakdown_colors[breakdown], 0);
                lv_obj_set_style_border_width(_funnel_segments[step][breakdown], 0, 0);
                lv_obj_set_style_radius(_funnel_segments[step][breakdown], 0, 0);
                lv_obj_set_style_pad_all(_funnel_segments[step][breakdown], 0, 0);
            }
            
            // Size and position the segment with no gaps
            lv_obj_set_size(_funnel_segments[step][breakdown], 
                           (int16_t)segmentWidth, 
                           FUNNEL_BAR_HEIGHT);
            lv_obj_align(_funnel_segments[step][breakdown], 
                        LV_ALIGN_LEFT_MID, 
                        (int16_t)currentWidth,
                        0);
            
            currentWidth += segmentWidth;
        }
        
        // Update yOffset for next step
        yOffset += FUNNEL_BAR_HEIGHT + FUNNEL_BAR_GAP;
    }
} 