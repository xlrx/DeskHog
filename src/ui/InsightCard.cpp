#include "InsightCard.h"
#include "Style.h"
#include "NumberFormat.h"
#include <algorithm>

// Helper class for memory-safe UI callbacks
class UICallback {
public:
    UICallback(std::function<void()> func) : _func(std::move(func)) {}
    
    void execute() {
        if (_func) {
            _func();
        }
    }
    
private:
    std::function<void()> _func;
};

InsightCard::InsightCard(lv_obj_t* parent, ConfigManager& config, EventQueue& eventQueue,
                        const String& insightId, uint16_t width, uint16_t height)
    : _config(config)
    , _event_queue(eventQueue)
    , _insight_id(insightId)
    , _card(nullptr)
    , _title_label(nullptr)
    , _content_container(nullptr)
    , _value_label(nullptr)
    , _chart(nullptr)
    , _series(nullptr)
    , _funnel_container(nullptr)
    , _current_type(InsightParser::InsightType::INSIGHT_NOT_SUPPORTED) {
    
    // Initialize arrays to nullptr
    for (int i = 0; i < MAX_FUNNEL_STEPS; i++) {
        _funnel_bars[i] = nullptr;
        _funnel_labels[i] = nullptr;
        for (int j = 0; j < MAX_BREAKDOWNS; j++) {
            _funnel_segments[i][j] = nullptr;
        }
    }
    
    // Initialize colors
    initBreakdownColors();
    
    // Create card
    _card = lv_obj_create(parent);
    if (!_card) return;
    
    lv_obj_set_size(_card, width, height);
    lv_obj_set_style_bg_color(_card, Style::backgroundColor(), 0);
    lv_obj_set_style_pad_all(_card, 0, 0);
    lv_obj_set_style_border_width(_card, 0, 0);
    lv_obj_set_style_radius(_card, 0, 0);
    
    // Create flex container for vertical layout
    lv_obj_t* flex_col = lv_obj_create(_card);
    if (!flex_col) return;
    
    lv_obj_set_size(flex_col, width, height);
    lv_obj_set_style_pad_row(flex_col, 5, 0);
    lv_obj_set_style_pad_all(flex_col, 5, 0);
    lv_obj_set_flex_flow(flex_col, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(flex_col, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(flex_col, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_opa(flex_col, LV_OPA_0, 0);
    lv_obj_set_style_border_width(flex_col, 0, 0);
    
    // Create title label
    _title_label = lv_label_create(flex_col);
    if (!_title_label) return;
    
    lv_obj_set_width(_title_label, width - 10);
    lv_obj_set_style_text_color(_title_label, Style::labelColor(), 0);
    lv_obj_set_style_text_font(_title_label, Style::labelFont(), 0);
    lv_label_set_long_mode(_title_label, LV_LABEL_LONG_DOT);
    lv_label_set_text(_title_label, "Loading...");
    
    // Create content container
    _content_container = lv_obj_create(flex_col);
    if (!_content_container) return;
    
    lv_obj_set_size(_content_container, width - 10, height - 40);
    lv_obj_set_style_bg_opa(_content_container, LV_OPA_0, 0);
    lv_obj_set_style_border_width(_content_container, 0, 0);
    lv_obj_set_style_pad_all(_content_container, 0, 0);
    
    // Create initial numeric elements
    createNumericElements();
    
    // Subscribe to events
    _event_queue.subscribe([this](const Event& event) {
        if (event.type == EventType::INSIGHT_DATA_RECEIVED && 
            event.insightId == _insight_id && 
            event.parser) {
            this->onEvent(event);
        }
    });
}

InsightCard::~InsightCard() {
    // Clean up UI elements safely
    dispatchToUI([this]() {
        if (isValidObject(_card)) {
            lv_obj_add_flag(_card, LV_OBJ_FLAG_HIDDEN);
            lv_obj_del_async(_card);
            
            // Clear all pointers
            _card = nullptr;
            _title_label = nullptr;
            _value_label = nullptr;
            _chart = nullptr;
            _series = nullptr;
            _funnel_container = nullptr;
            _content_container = nullptr;
            
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

bool InsightCard::isValidObject(lv_obj_t* obj) const {
    return obj && lv_obj_is_valid(obj);
}

void InsightCard::dispatchToUI(std::function<void()> update) {
    // Try immediate execution if possible
    if (isValidObject(_content_container)) {
        lv_display_t* disp = lv_obj_get_display(_content_container);
        if (disp) {
            lv_timer_handler();
            update();
            lv_display_send_event(disp, LV_EVENT_REFRESH, nullptr);
            lv_refr_now(disp);
            return;
        }
    }
    
    // Fall back to timer-based execution
    auto* callback = new UICallback(std::move(update));
    
    lv_timer_t* timer = lv_timer_create([](lv_timer_t* timer) {
        auto* cb = static_cast<UICallback*>(lv_timer_get_user_data(timer));
        if (cb) {
            cb->execute();
            delete cb;
        }
        lv_timer_del(timer);
    }, 0, callback);
}

void InsightCard::onEvent(const Event& event) {
    if (event.type == EventType::INSIGHT_DATA_RECEIVED && 
        event.insightId == _insight_id && 
        event.parser) {
        handleParsedData(event.parser);
    }
}

void InsightCard::handleParsedData(std::shared_ptr<InsightParser> parser) {
    if (!parser || !parser->isValid()) {
        updateNumericDisplay("Parse Error", 0);
        return;
    }
    
    auto insightType = parser->detectInsightType();
    
    char titleBuffer[64];
    if (!parser->getName(titleBuffer, sizeof(titleBuffer))) {
        strcpy(titleBuffer, "Unnamed Insight");
    }

    // Only save if title has changed
    if (_config.getInsight(_insight_id).compareTo(String(titleBuffer)) != 0) {
        _config.saveInsight(_insight_id, String(titleBuffer));
    }
    
    // Rebuild UI if type has changed
    if (insightType != _current_type) {
        _current_type = insightType;
        
        dispatchToUI([this, insightType]() {
            clearCardContent();
            
            switch (insightType) {
                case InsightParser::InsightType::NUMERIC_CARD:
                    createNumericElements();
                    break;
                case InsightParser::InsightType::LINE_GRAPH:
                    createLineGraphElements();
                    break;
                case InsightParser::InsightType::FUNNEL:
                    createFunnelElements();
                    break;
                default:
                    createNumericElements(); // Fallback
                    break;
            }
        });
    }
    
    // Update based on type
    switch (insightType) {
        case InsightParser::InsightType::NUMERIC_CARD:
            updateNumericDisplay(titleBuffer, parser->getNumericCardValue());
            break;
            
        case InsightParser::InsightType::LINE_GRAPH: {
            size_t pointCount = parser->getSeriesPointCount();
            if (pointCount > 0) {
                std::unique_ptr<double[]> values(new double[pointCount]);
                if (parser->getSeriesYValues(values.get())) {
                    updateLineGraphDisplay(titleBuffer, values.get(), pointCount);
                }
            }
            break;
        }
            
        case InsightParser::InsightType::FUNNEL:
            updateFunnelDisplay(titleBuffer, *parser);
            break;
            
        default:
            updateNumericDisplay("Unsupported Type", 0);
            break;
    }
}

void InsightCard::clearCardContent() {
    if (isValidObject(_content_container)) {
        lv_obj_clean(_content_container);
    }
    
    // Reset content pointers
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
    if (!isValidObject(_content_container)) return;
    
    _value_label = lv_label_create(_content_container);
    if (!_value_label) return;
    
    lv_obj_center(_value_label);
    lv_obj_set_style_text_font(_value_label, Style::largeValueFont(), 0);
    lv_obj_set_style_text_color(_value_label, Style::valueColor(), 0);
    lv_label_set_text(_value_label, "...");
}

void InsightCard::createLineGraphElements() {
    if (!isValidObject(_content_container)) return;
    
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
    
    lv_obj_set_style_size(_chart, 0, 0, LV_PART_INDICATOR);
    lv_obj_set_style_line_width(_chart, 2, LV_PART_ITEMS);
}

void InsightCard::createFunnelElements() {
    if (!isValidObject(_content_container)) return;
    
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

void InsightCard::formatNumber(double value, char* buffer, size_t bufferSize) {
    if (value >= 1000000) {
        // Display in millions (e.g., 1.2M)
        snprintf(buffer, bufferSize, "%.1fM", value / 1000000.0);
    } else if (value >= 100000) {
        // Display in thousands (e.g., 10.5K)
        snprintf(buffer, bufferSize, "%.1fK", value / 1000.0);
    } else if (value == (int)value) {
        // Integer with thousands separators
        char tempBuffer[32];
        snprintf(tempBuffer, sizeof(tempBuffer), "%.0f", value);
        NumberFormat::addThousandsSeparators(buffer, bufferSize, atoi(tempBuffer));
    } else if (value < 1) {
        // Small decimal values
        snprintf(buffer, bufferSize, "%.2f", value);
    } else {
        // Medium values
        snprintf(buffer, bufferSize, "%.1f", value);
    }
}

void InsightCard::updateNumericDisplay(const String& title, double value) {
    dispatchToUI([this, title = String(title), value]() {
        if (isValidObject(_title_label)) {
            lv_label_set_text(_title_label, title.c_str());
        }
        
        if (isValidObject(_value_label)) {
            char valueBuffer[32];
            formatNumber(value, valueBuffer, sizeof(valueBuffer));
            lv_label_set_text(_value_label, valueBuffer);
        }
    });
}

void InsightCard::updateLineGraphDisplay(const String& title, double* values, size_t pointCount) {
    if (!_chart || !_series) return;

    // Find max value for scaling
    double maxVal = 0;
    if (pointCount > 0) {
        maxVal = values[0];
        for (size_t i = 0; i < pointCount; i++) {
            if (values[i] > maxVal) maxVal = values[i];
        }
    }
    
    // Scale factor to keep values under 1000
    double scaleFactor = maxVal > 1000 ? 1000.0 / maxVal : 1.0;

    dispatchToUI([this, title = String(title), values, pointCount, maxVal, scaleFactor]() {
        if (!isValidObject(_chart)) return;
        
        if (isValidObject(_title_label)) {
            lv_label_set_text(_title_label, title.c_str());
        }
        
        // Limit to 20 points max for performance
        const size_t displayPoints = std::min(pointCount, static_cast<size_t>(20));
        
        // Update chart
        lv_chart_set_point_count(_chart, displayPoints);
        lv_chart_set_all_value(_chart, _series, 0);
        
        // Add points
        for (size_t i = 0; i < displayPoints; i++) {
            int16_t y_val = static_cast<int16_t>(values[i] * scaleFactor);
            lv_chart_set_value_by_id(_chart, _series, i, y_val);
        }
        
        // Set range
        lv_chart_set_range(_chart, LV_CHART_AXIS_PRIMARY_Y, 0, 
                          static_cast<int32_t>(maxVal * scaleFactor * 1.1));
        
        lv_chart_refresh(_chart);
    });
}

void InsightCard::updateFunnelDisplay(const String& title, InsightParser& parser) {
    if (!_funnel_container || !_title_label) return;
    
    size_t stepCount = std::min(parser.getFunnelStepCount(), 
                               static_cast<size_t>(MAX_FUNNEL_STEPS));
    size_t breakdownCount = std::min(parser.getFunnelBreakdownCount(), 
                                   static_cast<size_t>(MAX_BREAKDOWNS));
    
    if (stepCount == 0 || breakdownCount == 0) {
        return;
    }
    
    // Get raw data
    uint32_t stepCounts[MAX_FUNNEL_STEPS] = {0};
    double conversionRates[MAX_FUNNEL_STEPS-1] = {0.0};
    parser.getFunnelTotalCounts(0, stepCounts, conversionRates);
    
    uint32_t totalFirstStep = stepCounts[0];
    if (totalFirstStep == 0) return;  // Avoid division by zero
    
    // Pre-calculate step data
    struct StepData {
        uint32_t total;
        String label;
        std::vector<float> segmentWidths;
        std::vector<float> segmentOffsets;
    };
    
    std::vector<StepData> steps;
    steps.reserve(stepCount);
    lv_coord_t available_width = lv_obj_get_width(_content_container);
    
    // Process each step
    for (size_t step = 0; step < stepCount; step++) {
        StepData stepData;
        stepData.total = stepCounts[step];
        
        // Get breakdown data
        uint32_t breakdownCounts[MAX_BREAKDOWNS] = {0};
        double breakdownRates[MAX_BREAKDOWNS] = {0.0};
        parser.getFunnelBreakdownComparison(step, breakdownCounts, breakdownRates);
        
        // Get step name
        char stepNameBuffer[32] = {0};
        parser.getFunnelStepData(0, step, stepNameBuffer, sizeof(stepNameBuffer), 
                               nullptr, nullptr, nullptr);
        
        // Create label
        char numberBuffer[16];
        if (step == 0) {
            NumberFormat::addThousandsSeparators(numberBuffer, sizeof(numberBuffer), stepData.total);
            stepData.label = String(numberBuffer);
            if (stepNameBuffer[0] != '\0') {
                stepData.label += " - " + String(stepNameBuffer);
            }
        } else {
            NumberFormat::addThousandsSeparators(numberBuffer, sizeof(numberBuffer), stepData.total);
            uint32_t percentage = (stepData.total * 100) / totalFirstStep;
            stepData.label = String(numberBuffer) + " - " + String(percentage) + "%";
            if (stepNameBuffer[0] != '\0') {
                stepData.label += " - " + String(stepNameBuffer);
            }
        }
        
        // Calculate segments
        float stepPercentage = static_cast<float>(stepData.total) / totalFirstStep;
        float totalWidth = available_width * stepPercentage;
        float offset = 0;
        
        for (size_t i = 0; i < breakdownCount; i++) {
            float segmentPercentage = stepData.total > 0 ?
                static_cast<float>(breakdownCounts[i]) / stepData.total : 0.0f;
            
            float width = totalWidth * segmentPercentage;
            stepData.segmentWidths.push_back(width);
            stepData.segmentOffsets.push_back(offset);
            offset += width;
        }
        
        steps.push_back(std::move(stepData));
    }
    
    // Update UI
    dispatchToUI([this, title = String(title), steps = std::move(steps), 
                 breakdownCount]() {
        if (!isValidObject(_funnel_container) || !isValidObject(_title_label)) {
            return;
        }
        
        // Update title
        lv_label_set_text(_title_label, title.c_str());
        
        lv_coord_t available_width = lv_obj_get_width(_funnel_container);
        
        // Update steps
        int yOffset = 0;
        for (size_t step = 0; step < steps.size(); step++) {
            const auto& stepData = steps[step];
            
            // Create or update bar container
            if (!isValidObject(_funnel_bars[step])) {
                _funnel_bars[step] = lv_obj_create(_funnel_container);
                if (_funnel_bars[step]) {
                    lv_obj_set_size(_funnel_bars[step], available_width, FUNNEL_BAR_HEIGHT);
                    lv_obj_set_style_bg_opa(_funnel_bars[step], LV_OPA_0, 0);
                    lv_obj_set_style_border_width(_funnel_bars[step], 0, 0);
                    lv_obj_set_style_pad_all(_funnel_bars[step], 0, 0);
                    lv_obj_clear_flag(_funnel_bars[step], LV_OBJ_FLAG_SCROLLABLE);
                }
            }
            
            if (isValidObject(_funnel_bars[step])) {
                lv_obj_align(_funnel_bars[step], LV_ALIGN_TOP_LEFT, 0, yOffset);
                
                // Create or update label
                if (!isValidObject(_funnel_labels[step])) {
                    _funnel_labels[step] = lv_label_create(_funnel_container);
                    if (_funnel_labels[step]) {
                        lv_obj_set_style_text_color(_funnel_labels[step], Style::valueColor(), 0);
                        lv_obj_set_style_text_font(_funnel_labels[step], Style::valueFont(), 0);
                        lv_label_set_long_mode(_funnel_labels[step], LV_LABEL_LONG_DOT);
                        lv_obj_set_width(_funnel_labels[step], available_width);
                        lv_obj_set_height(_funnel_labels[step], FUNNEL_LABEL_HEIGHT);
                    }
                }
                
                if (isValidObject(_funnel_labels[step])) {
                    lv_label_set_text(_funnel_labels[step], stepData.label.c_str());
                    lv_obj_align(_funnel_labels[step], LV_ALIGN_TOP_LEFT, 1, 
                                yOffset + FUNNEL_BAR_HEIGHT + 2);
                }
                
                // Create or update segments
                for (size_t i = 0; i < stepData.segmentWidths.size() && i < breakdownCount; i++) {
                    if (!isValidObject(_funnel_segments[step][i])) {
                        _funnel_segments[step][i] = lv_obj_create(_funnel_bars[step]);
                        if (_funnel_segments[step][i]) {
                            lv_obj_set_style_bg_color(_funnel_segments[step][i],
                                                   _breakdown_colors[i], 0);
                            lv_obj_set_style_border_width(_funnel_segments[step][i], 0, 0);
                            lv_obj_set_style_radius(_funnel_segments[step][i], 0, 0);
                            lv_obj_set_style_pad_all(_funnel_segments[step][i], 0, 0);
                        }
                    }
                    
                    if (isValidObject(_funnel_segments[step][i])) {
                        // Ensure minimum width of 1px for visibility
                        int width = std::max(1, static_cast<int>(stepData.segmentWidths[i]));
                        lv_obj_set_size(_funnel_segments[step][i], width, FUNNEL_BAR_HEIGHT);
                        lv_obj_align(_funnel_segments[step][i], LV_ALIGN_LEFT_MID,
                                   static_cast<int>(stepData.segmentOffsets[i]), 0);
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

String InsightCard::getInsightId() const {
    return _insight_id;
}