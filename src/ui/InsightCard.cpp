#include "InsightCard.h"
#include "ColorScheme.h"
#include "NumberFormat.h"

#define GRAPH_WIDTH 240  // Full screen width
#define GRAPH_HEIGHT 90  // Height for the graph
#define FUNNEL_BAR_HEIGHT 7  // Reduced from 15 to 7
#define FUNNEL_BAR_GAP 20    // Increased to accommodate label
#define FUNNEL_LEFT_MARGIN 0  // No left margin
#define FUNNEL_LABEL_HEIGHT 20

void InsightCard::dispatchToUI(std::function<void()> update) {
    lv_timer_t* timer = lv_timer_create([](lv_timer_t* timer) {
        auto* callback = static_cast<std::function<void()>*>(timer->user_data);
        (*callback)();
        delete callback;
        lv_timer_del(timer);
    }, 0, new std::function<void()>(update));
}

InsightCard::InsightCard(lv_obj_t* parent, ConfigManager& config, PostHogClient& posthog_client,
                        const String& insightId, uint16_t width, uint16_t height)
    : _config(config)
    , _posthog_client(posthog_client)
    , _insight_id(insightId)
    , _card(nullptr)
    , _title_label(nullptr)
    , _value_label(nullptr)
    , _chart(nullptr)
    , _series(nullptr)
    , _funnel_container(nullptr)
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
    
    // Create initial UI elements synchronously since we're in setup()
    _card = lv_obj_create(parent);
    lv_obj_set_size(_card, width, height);
    lv_obj_set_style_bg_color(_card, ColorScheme::backgroundColor(), 0);
    lv_obj_set_style_pad_all(_card, 5, 0);
    
    // Create flex container for vertical layout
    lv_obj_t* flex_col = lv_obj_create(_card);
    lv_obj_set_size(flex_col, width - 10, height - 10); // Account for card padding
    lv_obj_set_style_pad_row(flex_col, 5, 0);  // 5px gap between items
    lv_obj_set_style_pad_top(flex_col, 0, 0);  // No padding at top
    lv_obj_set_flex_flow(flex_col, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(flex_col, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(flex_col, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_opa(flex_col, LV_OPA_0, 0);
    lv_obj_set_style_border_width(flex_col, 0, 0);
    
    // Create title label with wrapping
    _title_label = lv_label_create(flex_col);
    lv_obj_set_width(_title_label, width - 20); // Account for padding
    lv_obj_set_style_text_color(_title_label, ColorScheme::labelColor(), 0);
    lv_obj_set_style_text_font(_title_label, &lv_font_montserrat_14, 0);
    lv_label_set_long_mode(_title_label, LV_LABEL_LONG_WRAP);
    lv_label_set_text(_title_label, "Loading...");
    
    // Create content container that will hold either numeric display, chart, or funnel
    _content_container = lv_obj_create(flex_col);
    lv_obj_set_size(_content_container, width - 20, height - 40); // Adjust height to account for title
    lv_obj_set_style_bg_opa(_content_container, LV_OPA_0, 0);
    lv_obj_set_style_border_width(_content_container, 0, 0);
    lv_obj_set_style_pad_all(_content_container, 0, 0);
    
    // Create initial numeric display
    createNumericElements();
    
    // Subscribe to PostHog updates
    _posthog_client.subscribeToInsight(insightId, onDataReceived, this);
    
    // Queue an immediate fetch
    _posthog_client.queueRequest(insightId, onDataReceived, this);
}

InsightCard::~InsightCard() {
    _posthog_client.unsubscribeFromInsight(_insight_id);
    
    // Clean up UI elements on main thread
    dispatchToUI([this]() {
        if (_card) {
            lv_obj_del(_card);  // This will delete all child objects
            _card = nullptr;
            _title_label = nullptr;
            _value_label = nullptr;
            _chart = nullptr;
            _series = nullptr;
            _funnel_container = nullptr;
            
            for (int i = 0; i < MAX_FUNNEL_STEPS; i++) {
                _funnel_bars[i] = nullptr;
                _funnel_labels[i] = nullptr;
                for (int j = 0; j < MAX_BREAKDOWNS; j++) {
                    _funnel_segments[i][j] = nullptr;
                }
            }
        }
    });
}

lv_obj_t* InsightCard::getCard() {
    return _card;
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

    if (_config.getInsight(_insight_id).compareTo(String(titleBuffer)) != 0) {
        _config.saveInsight(_insight_id, String(titleBuffer));
    }
    
    // If type has changed, rebuild UI elements
    if (insightType != _current_type) {
        _current_type = insightType;
        
        dispatchToUI([this, insightType]() {
            clearCardContent();
            
            if (insightType == InsightParser::InsightType::NUMERIC_CARD) {
                createNumericElements();
            } else if (insightType == InsightParser::InsightType::LINE_GRAPH) {
                createLineGraphElements();
            } else if (insightType == InsightParser::InsightType::FUNNEL) {
                createFunnelElements();
            }
        });
    }
    
    // Update based on type
    if (insightType == InsightParser::InsightType::NUMERIC_CARD) {
        double value = parser.getNumericCardValue();
        updateNumericDisplay(titleBuffer, value);
    } 
    else if (insightType == InsightParser::InsightType::LINE_GRAPH) {
        size_t pointCount = parser.getSeriesPointCount();
        if (pointCount > 0) {
            std::unique_ptr<double[]> values(new double[pointCount]);
            if (parser.getSeriesYValues(values.get())) {
                updateLineGraphDisplay(titleBuffer, values.get(), pointCount);
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
    // This is called from dispatchToUI, so we're already on the main thread
    if (_content_container && lv_obj_is_valid(_content_container)) {
        lv_obj_clean(_content_container);
    }
    
    // Reset all content pointers since they were children of the cleaned container
    _value_label = nullptr;
    _chart = nullptr;
    _series = nullptr;
    _funnel_container = nullptr;
    
    for (int i = 0; i < MAX_FUNNEL_STEPS; i++) {
        _funnel_bars[i] = nullptr;
        _funnel_labels[i] = nullptr;
        for (int j = 0; j < MAX_BREAKDOWNS; j++) {
            _funnel_segments[i][j] = nullptr;
        }
    }
}

void InsightCard::createNumericElements() {
    if (!_content_container || !lv_obj_is_valid(_content_container)) return;
    
    // Clean up any existing value label
    if (_value_label) {
        lv_obj_del(_value_label);
        _value_label = nullptr;
    }
    
    _value_label = lv_label_create(_content_container);
    if (!_value_label) return;
    
    lv_obj_center(_value_label);
    lv_obj_set_style_text_font(_value_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(_value_label, ColorScheme::valueColor(), 0);
    lv_label_set_text(_value_label, "...");
}

void InsightCard::createLineGraphElements() {
    if (!_content_container || !lv_obj_is_valid(_content_container)) return;
    
    // Clean up any existing chart
    if (_chart) {
        lv_obj_del(_chart);
        _chart = nullptr;
        _series = nullptr;
    }
    
    _chart = lv_chart_create(_content_container);
    if (!_chart) return;
    
    lv_obj_set_size(_chart, GRAPH_WIDTH, GRAPH_HEIGHT);
    lv_obj_center(_chart);
    lv_chart_set_type(_chart, LV_CHART_TYPE_LINE);
    
    _series = lv_chart_add_series(_chart, lv_color_hex(0x2980b9), LV_CHART_AXIS_PRIMARY_Y);
    if (!_series) {
        lv_obj_del(_chart);
        _chart = nullptr;
        return;
    }
    
    lv_obj_set_style_size(_chart, 0, LV_PART_INDICATOR);
    lv_obj_set_style_line_width(_chart, 2, LV_PART_ITEMS);
}

void InsightCard::createFunnelElements() {
    if (!_content_container || !lv_obj_is_valid(_content_container)) return;
    
    // Clean up any existing funnel container
    if (_funnel_container) {
        lv_obj_del(_funnel_container);
        _funnel_container = nullptr;
        
        // Reset all funnel-related pointers since they were children of the deleted container
        for (int i = 0; i < MAX_FUNNEL_STEPS; i++) {
            _funnel_bars[i] = nullptr;
            _funnel_labels[i] = nullptr;
            for (int j = 0; j < MAX_BREAKDOWNS; j++) {
                _funnel_segments[i][j] = nullptr;
            }
        }
    }
    
    // Create new funnel container
    _funnel_container = lv_obj_create(_content_container);
    if (!_funnel_container) return;
    
    lv_coord_t available_width = lv_obj_get_width(_content_container);
    lv_obj_set_size(_funnel_container, available_width, GRAPH_HEIGHT);
    lv_obj_align(_funnel_container, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_clear_flag(_funnel_container, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_pad_all(_funnel_container, 0, 0);
    lv_obj_set_style_border_width(_funnel_container, 0, 0);
    lv_obj_set_style_bg_opa(_funnel_container, LV_OPA_0, 0);
}

void InsightCard::updateNumericDisplay(const String& title, double value) {
    dispatchToUI([this, title = String(title), value]() {
        if (_title_label) {
            lv_label_set_text(_title_label, title.c_str());
        }
        
        if (_value_label) {
            char valueBuffer[32];
            if (value == (int)value) {
                snprintf(valueBuffer, sizeof(valueBuffer), "%.0f", value);
            } else {
                snprintf(valueBuffer, sizeof(valueBuffer), "%.2f", value);
            }
            lv_label_set_text(_value_label, valueBuffer);
        }
    });
}

void InsightCard::updateLineGraphDisplay(const String& title, double* values, size_t pointCount) {
    if (!_chart || !_series) return;

    Serial.printf("\n=== Chart Update ===\n");
    Serial.printf("Point count: %d\n", pointCount);
    Serial.printf("Chart width: %d\n", GRAPH_WIDTH);
    
    // Find max value for scaling - do this outside dispatch since it's just data processing
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

    // Pre-calculate all scaled values
    std::vector<int32_t> scaledValues;
    scaledValues.reserve(pointCount);
    for(size_t i = 0; i < pointCount; i++) {
        scaledValues.push_back((int32_t)(values[i] * scaleFactor));
    }

    // Do all LVGL operations in a single dispatch to maintain exact ordering
    dispatchToUI([this, title = String(title), pointCount, scaledValues = std::move(scaledValues)]() {
        if (!_chart || !_series || !_title_label) return;
        
        // Update exactly in original order
        lv_label_set_text(_title_label, title.c_str());
        lv_chart_set_point_count(_chart, pointCount);
        
        // Add all points in sequence
        for(int32_t value : scaledValues) {
            lv_chart_set_next_value(_chart, _series, value);
        }
        
        lv_chart_refresh(_chart);
        Serial.println("Chart updated\n");
    });
}

void InsightCard::updateFunnelDisplay(const String& title, InsightParser& parser) {
    if (!_funnel_container || !_title_label) return;
    
    size_t stepCount = parser.getFunnelStepCount();
    size_t breakdownCount = parser.getFunnelBreakdownCount();
    
    if (stepCount == 0 || breakdownCount == 0) {
        return;
    }
    
    // Limit to maximum supported steps and breakdowns
    stepCount = min(stepCount, (size_t)MAX_FUNNEL_STEPS);
    breakdownCount = min(breakdownCount, (size_t)MAX_BREAKDOWNS);
    
    // Pre-calculate all step data
    struct StepData {
        uint32_t total;
        String label;
        std::vector<float> segmentWidths;
        std::vector<float> segmentOffsets;
    };
    
    std::vector<StepData> stepDataArray;
    stepDataArray.reserve(stepCount);
    
    lv_coord_t available_width = lv_obj_get_width(_funnel_container);
    
    // Get total counts for first step to calculate percentages
    uint32_t stepCounts[MAX_FUNNEL_STEPS] = {0};
    double conversionRates[MAX_FUNNEL_STEPS-1] = {0.0};
    parser.getFunnelTotalCounts(0, stepCounts, conversionRates);
    
    uint32_t totalFirstStep = stepCounts[0];
    
    // Calculate all step data outside the dispatch
    for (size_t step = 0; step < stepCount; step++) {
        StepData stepData;
        
        // Get breakdown data for this step
        uint32_t breakdownCounts[MAX_BREAKDOWNS] = {0};
        double breakdownRates[MAX_BREAKDOWNS] = {0.0};
        parser.getFunnelBreakdownComparison(step, breakdownCounts, breakdownRates);
        
        // Get total for this step
        stepData.total = stepCounts[step];
        
        // Get step name
        char stepNameBuffer[32] = {0};
        parser.getFunnelStepData(0, step, stepNameBuffer, sizeof(stepNameBuffer), nullptr, nullptr, nullptr);
        
        // Create label text
        char numberBuffer[16];
        if (step == 0) {
            NumberFormat::addThousandsSeparators(numberBuffer, sizeof(numberBuffer), stepData.total);
            stepData.label = String(numberBuffer);
            if (stepNameBuffer[0] != '\0') {
                stepData.label += " - " + String(stepNameBuffer);
            }
        } else {
            NumberFormat::addThousandsSeparators(numberBuffer, sizeof(numberBuffer), stepData.total);
            uint32_t percentage = totalFirstStep > 0 ? (stepData.total * 100) / totalFirstStep : 0;
            stepData.label = String(numberBuffer) + " - " + String(percentage) + "%";
            if (stepNameBuffer[0] != '\0') {
                stepData.label += " - " + String(stepNameBuffer);
            }
        }
        
        // Calculate segment widths and offsets
        float stepPercentage = (float)stepData.total / totalFirstStep;
        float totalWidth = available_width * stepPercentage;
        float currentWidth = 0;
        
        for (size_t breakdown = 0; breakdown < breakdownCount; breakdown++) {
            float segmentPercentage = 0.0f;
            if (stepData.total > 0) {
                segmentPercentage = (float)breakdownCounts[breakdown] / stepData.total;
            } else {
                segmentPercentage = 0.0f;
            }
            
            float segmentWidth = totalWidth * segmentPercentage;
            
            stepData.segmentWidths.push_back(segmentWidth);
            stepData.segmentOffsets.push_back(currentWidth);
            
            currentWidth += segmentWidth;
        }
        
        stepDataArray.push_back(std::move(stepData));
    }
    
    // Single dispatch for all UI updates
    dispatchToUI([this, title = String(title), stepCount, breakdownCount, 
                  stepDataArray = std::move(stepDataArray), available_width]() {
        if (!_funnel_container || !_title_label) return;
        
        // Update title
        lv_label_set_text(_title_label, title.c_str());
        
        // Update each step
        int yOffset = 0;
        for (size_t step = 0; step < stepCount; step++) {
            const auto& stepData = stepDataArray[step];
            
            // Create or ensure bar container exists
            if (!_funnel_bars[step]) {
                _funnel_bars[step] = lv_obj_create(_funnel_container);
                if (_funnel_bars[step]) {
                    lv_obj_set_size(_funnel_bars[step], available_width, FUNNEL_BAR_HEIGHT);
                    lv_obj_set_style_bg_opa(_funnel_bars[step], LV_OPA_0, 0);
                    lv_obj_set_style_border_width(_funnel_bars[step], 0, 0);
                    lv_obj_set_style_pad_all(_funnel_bars[step], 0, 0);
                    lv_obj_clear_flag(_funnel_bars[step], LV_OBJ_FLAG_SCROLLABLE);
                }
            }
            
            if (_funnel_bars[step]) {
                lv_obj_align(_funnel_bars[step], LV_ALIGN_TOP_LEFT, 0, yOffset);
                
                // Create or update label
                if (!_funnel_labels[step]) {
                    _funnel_labels[step] = lv_label_create(_funnel_container);
                    if (_funnel_labels[step]) {
                        lv_obj_set_style_text_align(_funnel_labels[step], LV_TEXT_ALIGN_LEFT, 0);
                        lv_obj_set_style_text_font(_funnel_labels[step], &lv_font_montserrat_14, 0);
                        lv_obj_set_style_text_color(_funnel_labels[step], ColorScheme::labelColor(), 0);
                    }
                }
                
                if (_funnel_labels[step]) {
                    lv_label_set_text(_funnel_labels[step], stepData.label.c_str());
                    lv_obj_align(_funnel_labels[step], LV_ALIGN_TOP_LEFT, 1, 
                                yOffset + FUNNEL_BAR_HEIGHT + 2);
                }
                
                // Create or update segments
                for (size_t breakdown = 0; breakdown < stepData.segmentWidths.size(); breakdown++) {
                    if (!_funnel_segments[step][breakdown]) {
                        _funnel_segments[step][breakdown] = lv_obj_create(_funnel_bars[step]);
                        if (_funnel_segments[step][breakdown]) {
                            lv_obj_set_style_bg_color(_funnel_segments[step][breakdown],
                                                    _breakdown_colors[breakdown], 0);
                            lv_obj_set_style_border_width(_funnel_segments[step][breakdown], 0, 0);
                            lv_obj_set_style_radius(_funnel_segments[step][breakdown], 0, 0);
                            lv_obj_set_style_pad_all(_funnel_segments[step][breakdown], 0, 0);
                        }
                    }
                    
                    if (_funnel_segments[step][breakdown]) {
                        lv_obj_set_size(_funnel_segments[step][breakdown],
                                      (int16_t)stepData.segmentWidths[breakdown],
                                      FUNNEL_BAR_HEIGHT);
                        lv_obj_align(_funnel_segments[step][breakdown],
                                   LV_ALIGN_LEFT_MID,
                                   (int16_t)stepData.segmentOffsets[breakdown],
                                   0);
                    }
                }
            }
            
            yOffset += FUNNEL_BAR_HEIGHT + FUNNEL_BAR_GAP;
        }
    });
}

void InsightCard::initBreakdownColors() {
    _breakdown_colors[0] = lv_color_hex(0x2980b9);  // Blue
    _breakdown_colors[1] = lv_color_hex(0x27ae60);  // Green
    _breakdown_colors[2] = lv_color_hex(0x8e44ad);  // Purple
    _breakdown_colors[3] = lv_color_hex(0xd35400);  // Orange
    _breakdown_colors[4] = lv_color_hex(0xc0392b);  // Red
} 