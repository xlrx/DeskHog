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

QueueHandle_t InsightCard::uiQueue = nullptr;

void InsightCard::initUIQueue() {
    // Create a queue that can hold up to 10 update requests
    uiQueue = xQueueCreate(10, sizeof(UICallback*));
}

void InsightCard::processUIQueue() {
    // Process all pending UI updates (call this from lvglHandlerTask)
    UICallback* callback = nullptr;
    static int queueCount = 0;
    int processed = 0;
    
    while(xQueueReceive(uiQueue, &callback, 0) == pdTRUE) {
        if (callback) {
            processed++;
            Serial.printf("[UI-DEBUG] Processing callback #%d\n", ++queueCount);
            
            callback->execute();
            
            // Force immediate refresh
            lv_display_t* disp = lv_display_get_default();
            if (disp) {
                lv_refr_now(disp);
            }
            
            delete callback;
        }
    }
    
    if (processed > 0) {
        Serial.printf("[UI-DEBUG] Processed %d callbacks from UI queue\n", processed);
    }
}

void InsightCard::dispatchToUI(std::function<void()> update) {
    static int dispatchCount = 0;
    dispatchCount++;
    
    // Create a callback object to hold the update function
    UICallback* callback = new UICallback(std::move(update));
    
    Serial.printf("[UI-DEBUG] Dispatching UI update #%d from Core %d\n", 
                 dispatchCount, xPortGetCoreID());
    
    // Try to send to queue
    if (xQueueSend(uiQueue, &callback, 0) != pdTRUE) {
        // Queue is full, handle error
        Serial.println("[UI-DEBUG] UI queue full, update discarded");
        delete callback;
    }
}

// Static version for general dispatching
void InsightCard::dispatchToLVGLTask(std::function<void()> update) {
    static int dispatchCount = 0;
    dispatchCount++;
    
    // Create a callback object to hold the update function
    UICallback* callback = new UICallback(std::move(update));
    
    Serial.printf("[UI-DEBUG] Dispatching static UI update #%d from Core %d\n", 
                 dispatchCount, xPortGetCoreID());
    
    // Try to send to queue
    if (xQueueSend(uiQueue, &callback, 0) != pdTRUE) {
        // Queue is full, handle error
        Serial.println("[UI-DEBUG] Static UI queue full, update discarded");
        delete callback;
    }
}

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
            event.insightId == _insight_id) {
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

void InsightCard::onEvent(const Event& event) {

    Serial.printf("onEvent called from Core: %d, Task: %s\n", 
                  xPortGetCoreID(), 
                  pcTaskGetTaskName(NULL));

    if (event.type == EventType::INSIGHT_DATA_RECEIVED && 
        event.insightId == _insight_id) {
        
        // If the event contains raw JSON, parse it in this thread
        if (event.jsonData.length() > 0) {
            // Log JSON size for debugging
            if (event.jsonData.length() > 8192) { // 8KB threshold
                Serial.printf("Processing large JSON (%u bytes) for insight %s\n", 
                            event.jsonData.length(), _insight_id.c_str());
            }
            
            // Debug insight type
            Serial.printf("[FUNNEL-DEBUG] Received event for insight %s\n", _insight_id.c_str());
            
            // Create a local parser on the UI thread
            // This keeps all parser operations in the same thread as UI updates
            auto parser = std::make_shared<InsightParser>(event.jsonData.c_str());
            if (parser->isValid()) {
                auto insightType = parser->detectInsightType();
                Serial.printf("[FUNNEL-DEBUG] Detected insight type: %d\n", (int)insightType);
                
                // Check if this is a funnel
                if (insightType == InsightParser::InsightType::FUNNEL) {
                    Serial.println("[FUNNEL-DEBUG] Funnel insight detected");
                    
                    // Log funnel details
                    size_t stepCount = parser->getFunnelStepCount();
                    size_t breakdownCount = parser->getFunnelBreakdownCount();
                    Serial.printf("[FUNNEL-DEBUG] Raw funnel data: %d steps, %d breakdowns\n", 
                                stepCount, breakdownCount);
                }
                
                // Check if UI elements are ready
                Serial.printf("[FUNNEL-DEBUG] UI container ready: %s\n", 
                            isValidObject(_funnel_container) ? "YES" : "NO");
                
                // Proceed with handling
                handleParsedData(parser);
            } else {
                Serial.printf("Failed to parse JSON for insight %s\n", _insight_id.c_str());
                updateNumericDisplay("Parse Error", 0);
            }
        }
        // Backward compatibility for events with parser already included
        else if (event.parser) {
            handleParsedData(event.parser);
        }
    }
}

void InsightCard::handleParsedData(std::shared_ptr<InsightParser> parser) {
    if (!parser || !parser->isValid()) {
        updateNumericDisplay("Parse Error", 0);
        return;
    }
    
    auto insightType = parser->detectInsightType();
    Serial.printf("[FUNNEL-DEBUG] handleParsedData - detected type: %d\n", (int)insightType);
    
    char titleBuffer[64];
    if (!parser->getName(titleBuffer, sizeof(titleBuffer))) {
        strcpy(titleBuffer, "Unnamed Insight");
    }

    // Only save if title has changed
    if (_config.getInsight(_insight_id).compareTo(String(titleBuffer)) != 0) {
        _config.saveInsight(_insight_id, String(titleBuffer));
    }
    
    // Critical section for funnel rendering issue
    if (insightType == InsightParser::InsightType::FUNNEL) {
        // Check if we have a funnel container already
        bool containerExists = isValidObject(_funnel_container);
        Serial.printf("[FUNNEL-DEBUG] Funnel container exists: %s\n", containerExists ? "YES" : "NO");
    }
    
    // Rebuild UI if type has changed
    if (insightType != _current_type) {
        Serial.printf("[FUNNEL-DEBUG] Type changed from %d to %d, rebuilding UI\n", 
                    (int)_current_type, (int)insightType);
        _current_type = insightType;
        
        // CRITICAL FIX: For funnels, we need to ensure UI elements are created before updating
        if (insightType == InsightParser::InsightType::FUNNEL) {
            // Try to create the funnel elements immediately on the UI thread
            Serial.println("[FUNNEL-DEBUG] Creating funnel elements IMMEDIATELY");
            dispatchToUI([this]() {
                clearCardContent();
                createFunnelElements();
                
                // Force immediate refresh
                lv_obj_invalidate(_content_container);
                lv_display_t* disp = lv_display_get_default();
                if (disp) {
                    lv_refr_now(disp);
                }
                
                Serial.println("[FUNNEL-DEBUG] Funnel elements created on UI thread");
            });
            
        }
        else {
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
                        Serial.println("[FUNNEL-DEBUG] Creating funnel elements via normal flow");
                        createFunnelElements();
                        break;
                    default:
                        createNumericElements(); // Fallback
                        break;
                }
            });
        }
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
            Serial.println("[FUNNEL-DEBUG] Updating funnel display");
            
            // Always verify container exists before updating
            if (!isValidObject(_funnel_container)) {
                Serial.println("[FUNNEL-DEBUG] CRITICAL ERROR: Funnel container missing before update!");
                
                // Try to recreate the container
                dispatchToUI([this]() {
                    Serial.println("[FUNNEL-DEBUG] Emergency creating funnel elements");
                    createFunnelElements();
                });
                
                // Wait for UI update
                delay(50);
            }
            
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
    lv_obj_set_style_bg_color(_chart, lv_color_hex(0x1A1A1A), 0); // Dark grey background
    lv_obj_set_style_border_width(_chart, 0, 0); // Match other elements
    
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
    Serial.println("[FUNNEL-DEBUG] Entry createFunnelElements()");
    
    if (!isValidObject(_content_container)) {
        Serial.println("[FUNNEL-DEBUG] Content container invalid");
        return;
    }
    
    _funnel_container = lv_obj_create(_content_container);
    if (!_funnel_container) {
        Serial.println("[FUNNEL-DEBUG] Failed to create funnel container");
        return;
    }
    
    lv_coord_t available_width = lv_obj_get_width(_content_container);
    lv_obj_set_size(_funnel_container, available_width, GRAPH_HEIGHT);
    lv_obj_align(_funnel_container, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_clear_flag(_funnel_container, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_pad_all(_funnel_container, 0, 0);
    lv_obj_set_style_border_width(_funnel_container, 0, 0);
    lv_obj_set_style_bg_opa(_funnel_container, LV_OPA_0, 0);
    
    Serial.printf("[FUNNEL-DEBUG] Creating %d funnel steps with %d breakdowns each\n", 
                 MAX_FUNNEL_STEPS, MAX_BREAKDOWNS);
    
    // Immediately create placeholder elements to ensure first render works
    for (int i = 0; i < MAX_FUNNEL_STEPS; i++) {
        // Create bar container
        _funnel_bars[i] = lv_obj_create(_funnel_container);
        if (_funnel_bars[i]) {
            lv_obj_set_size(_funnel_bars[i], available_width, FUNNEL_BAR_HEIGHT);
            lv_obj_set_style_bg_opa(_funnel_bars[i], LV_OPA_0, 0);
            lv_obj_set_style_border_width(_funnel_bars[i], 0, 0);
            lv_obj_set_style_pad_all(_funnel_bars[i], 0, 0);
            lv_obj_clear_flag(_funnel_bars[i], LV_OBJ_FLAG_SCROLLABLE);
            lv_obj_add_flag(_funnel_bars[i], LV_OBJ_FLAG_HIDDEN); // Hide until data arrives
            
            // Create label
            _funnel_labels[i] = lv_label_create(_funnel_container);
            if (_funnel_labels[i]) {
                lv_obj_set_style_text_color(_funnel_labels[i], Style::valueColor(), 0);
                lv_obj_set_style_text_font(_funnel_labels[i], Style::valueFont(), 0);
                lv_label_set_long_mode(_funnel_labels[i], LV_LABEL_LONG_DOT);
                lv_obj_set_width(_funnel_labels[i], available_width);
                lv_obj_set_height(_funnel_labels[i], FUNNEL_LABEL_HEIGHT);
                lv_obj_add_flag(_funnel_labels[i], LV_OBJ_FLAG_HIDDEN); // Hide until data arrives
            }
            
            // Create segments
            for (int j = 0; j < MAX_BREAKDOWNS; j++) {
                _funnel_segments[i][j] = lv_obj_create(_funnel_bars[i]);
                if (_funnel_segments[i][j]) {
                    lv_obj_set_style_bg_color(_funnel_segments[i][j], _breakdown_colors[j], 0);
                    lv_obj_set_style_border_width(_funnel_segments[i][j], 0, 0);
                    lv_obj_set_style_radius(_funnel_segments[i][j], 0, 0);
                    lv_obj_set_style_pad_all(_funnel_segments[i][j], 0, 0);
                    lv_obj_add_flag(_funnel_segments[i][j], LV_OBJ_FLAG_HIDDEN); // Hide until data arrives
                }
            }
        }
    }
    
    Serial.println("[FUNNEL-DEBUG] Funnel elements created successfully");
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
        const size_t displayPoints = pointCount;
        
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
    // Start debug for this update
    Serial.printf("[FUNNEL-DEBUG] updateFunnelDisplay called for %s\n", title.c_str());

    // Early validity checks
    if (!_funnel_container) {
        Serial.println("[FUNNEL-DEBUG] Funnel container not created");
        return;
    }
    
    // Get funnel data
    size_t stepCount = std::min(parser.getFunnelStepCount(), 
                               static_cast<size_t>(MAX_FUNNEL_STEPS));
    size_t breakdownCount = std::min(parser.getFunnelBreakdownCount(), 
                                    static_cast<size_t>(MAX_BREAKDOWNS));
    
    Serial.printf("[FUNNEL-DEBUG] Processing funnel with %d steps and %d breakdowns\n", 
                 stepCount, breakdownCount);
    
    if (stepCount == 0) {
        Serial.println("[FUNNEL-DEBUG] No funnel steps found");
        return;
    }
    
    // Get raw data
    uint32_t stepCounts[MAX_FUNNEL_STEPS] = {0};
    double conversionRates[MAX_FUNNEL_STEPS-1] = {0.0};
    if (!parser.getFunnelTotalCounts(0, stepCounts, conversionRates)) {
        Serial.println("[FUNNEL-DEBUG] Failed to get funnel total counts");
        return;
    }
    
    // Debug step counts
    Serial.print("[FUNNEL-DEBUG] Step counts: ");
    for (size_t i = 0; i < stepCount; i++) {
        Serial.printf("%d ", stepCounts[i]);
    }
    Serial.println();
    
    uint32_t totalFirstStep = stepCounts[0];
    if (totalFirstStep == 0) {
        Serial.println("[FUNNEL-DEBUG] First step count is zero");
        return;  // Avoid division by zero
    }
    
    // Prepare funnel data before UI update
    struct FunnelStep {
        String label;
        uint32_t total;
        float relativeWidth; // 0.0-1.0 relative to first step
        struct {
            float width; // Actual pixel width
            float offset; // X position offset
        } segments[MAX_BREAKDOWNS];
    };
    
    FunnelStep steps[MAX_FUNNEL_STEPS];
    lv_coord_t available_width = lv_obj_get_width(_content_container);
    
    Serial.println("[FUNNEL-DEBUG] Preparing funnel data structure");
    
    for (size_t step = 0; step < stepCount; step++) {
        // Get step data
        FunnelStep& currentStep = steps[step];
        currentStep.total = stepCounts[step];
        currentStep.relativeWidth = static_cast<float>(currentStep.total) / totalFirstStep;
        
        // Get step name
        char stepNameBuffer[32] = {0};
        parser.getFunnelStepData(0, step, stepNameBuffer, sizeof(stepNameBuffer), 
                               nullptr, nullptr, nullptr);
        
        // Create label
        char numberBuffer[16];
        NumberFormat::addThousandsSeparators(numberBuffer, sizeof(numberBuffer), currentStep.total);
        
        if (step == 0) {
            currentStep.label = String(numberBuffer);
            if (stepNameBuffer[0] != '\0') {
                currentStep.label += " - " + String(stepNameBuffer);
            }
        } else {
            uint32_t percentage = (currentStep.total * 100) / totalFirstStep;
            currentStep.label = String(numberBuffer) + " - " + String(percentage) + "%";
            if (stepNameBuffer[0] != '\0') {
                currentStep.label += " - " + String(stepNameBuffer);
            }
        }
        
        // Calculate breakdowns
        uint32_t breakdownCounts[MAX_BREAKDOWNS] = {0};
        double breakdownRates[MAX_BREAKDOWNS] = {0.0};
        
        if (parser.getFunnelBreakdownComparison(step, breakdownCounts, breakdownRates)) {
            float totalWidth = available_width * currentStep.relativeWidth;
            float offset = 0;
            
            for (size_t i = 0; i < breakdownCount; i++) {
                float segmentPercentage = currentStep.total > 0 ?
                    static_cast<float>(breakdownCounts[i]) / currentStep.total : 0.0f;
                
                float width = totalWidth * segmentPercentage;
                
                currentStep.segments[i].width = width;
                currentStep.segments[i].offset = offset;
                
                offset += width;
            }
        }
    }
    
    // Update UI
    Serial.println("[FUNNEL-DEBUG] Dispatching funnel UI update lambda");
    
    dispatchToUI([this, title = String(title), steps, stepCount, breakdownCount]() {
        Serial.println("[FUNNEL-DEBUG] Started UI update lambda execution");
        
        if (!isValidObject(_funnel_container) || !isValidObject(_title_label)) {
            Serial.println("[FUNNEL-DEBUG] Invalid UI objects when updating funnel");
            return;
        }
        
        // Update title
        lv_label_set_text(_title_label, title.c_str());
        Serial.printf("[FUNNEL-DEBUG] Set title to: %s\n", title.c_str());
        
        // Update steps
        int yOffset = 0;
        
        for (size_t step = 0; step < stepCount; step++) {
            const auto& stepData = steps[step];
            
            Serial.printf("[FUNNEL-DEBUG] Updating step %d, label: %s\n", 
                        (int)step, stepData.label.c_str());
            
            if (isValidObject(_funnel_bars[step])) {
                // Position the bar
                lv_obj_align(_funnel_bars[step], LV_ALIGN_TOP_LEFT, 0, yOffset);
                lv_obj_clear_flag(_funnel_bars[step], LV_OBJ_FLAG_HIDDEN);
                
                // Update label
                if (isValidObject(_funnel_labels[step])) {
                    lv_label_set_text(_funnel_labels[step], stepData.label.c_str());
                    lv_obj_align(_funnel_labels[step], LV_ALIGN_TOP_LEFT, 1, 
                               yOffset + FUNNEL_BAR_HEIGHT + 2);
                    lv_obj_clear_flag(_funnel_labels[step], LV_OBJ_FLAG_HIDDEN);
                    Serial.printf("[FUNNEL-DEBUG] Step %d label displayed\n", (int)step);
                }
                
                // Update segments
                for (size_t i = 0; i < breakdownCount; i++) {
                    if (isValidObject(_funnel_segments[step][i])) {
                        // Ensure minimum width of 1px for visibility
                        int width = std::max(1, static_cast<int>(stepData.segments[i].width));
                        lv_obj_set_size(_funnel_segments[step][i], width, FUNNEL_BAR_HEIGHT);
                        lv_obj_align(_funnel_segments[step][i], LV_ALIGN_LEFT_MID,
                                   static_cast<int>(stepData.segments[i].offset), 0);
                        
                        // Only show if width > 0
                        if (width > 0) {
                            lv_obj_clear_flag(_funnel_segments[step][i], LV_OBJ_FLAG_HIDDEN);
                        } else {
                            lv_obj_add_flag(_funnel_segments[step][i], LV_OBJ_FLAG_HIDDEN);
                        }
                    }
                }
                
                // Hide unused segments
                for (size_t i = breakdownCount; i < MAX_BREAKDOWNS; i++) {
                    if (isValidObject(_funnel_segments[step][i])) {
                        lv_obj_add_flag(_funnel_segments[step][i], LV_OBJ_FLAG_HIDDEN);
                    }
                }
            } else {
                Serial.printf("[FUNNEL-DEBUG] ERROR: Funnel bar %d is invalid!\n", (int)step);
            }
            
            yOffset += FUNNEL_BAR_HEIGHT + FUNNEL_BAR_GAP;
        }
        
        // Hide unused steps
        for (size_t step = stepCount; step < MAX_FUNNEL_STEPS; step++) {
            if (isValidObject(_funnel_bars[step])) {
                lv_obj_add_flag(_funnel_bars[step], LV_OBJ_FLAG_HIDDEN);
            }
            if (isValidObject(_funnel_labels[step])) {
                lv_obj_add_flag(_funnel_labels[step], LV_OBJ_FLAG_HIDDEN);
            }
        }
        
        // Force an immediate refresh of the display
        lv_obj_invalidate(_funnel_container);
        lv_display_t* disp = lv_display_get_default();
        if (disp) {
            lv_refr_now(disp);
            Serial.println("[FUNNEL-DEBUG] Forced immediate refresh");
        }
        
        Serial.println("[FUNNEL-DEBUG] Completed UI update lambda execution");
    });
    
    Serial.println("[FUNNEL-DEBUG] updateFunnelDisplay complete");
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