#include "LineGraphRenderer.h"
#include <memory> // For std::unique_ptr for data arrays
#include <algorithm> // For std::min

LineGraphRenderer::LineGraphRenderer()
    : _chart(nullptr), _series(nullptr) {
    // Serial.println("[LineGraphRenderer] Constructor");
}

LineGraphRenderer::~LineGraphRenderer() {
    // Serial.println("[LineGraphRenderer] Destructor");
    // Relies on InsightCard calling clearElements before destruction.
}

void LineGraphRenderer::createElements(lv_obj_t* parent_container) {
    if (!isValidLVGLObject(parent_container)) {
        Serial.println("[LineGraphRenderer-ERROR] Parent container invalid in createElements.");
        return;
    }

    // Use parent_container dimensions for the chart
    lv_coord_t container_width = lv_obj_get_content_width(parent_container); // Use content width to respect padding
    lv_coord_t container_height = lv_obj_get_content_height(parent_container);

    _chart = lv_chart_create(parent_container);
    if (!_chart) {
        Serial.println("[LineGraphRenderer-ERROR] Failed to create chart object.");
        return;
    }

    lv_obj_set_size(_chart, container_width, container_height);
    lv_obj_align(_chart, LV_ALIGN_CENTER, 0, 0); // Center in parent
    lv_chart_set_type(_chart, LV_CHART_TYPE_LINE);
    lv_obj_clear_flag(_chart, LV_OBJ_FLAG_SCROLLABLE); // Ensure no scrollbars

    // Styling from previous implementation
    lv_obj_set_style_bg_color(_chart, lv_color_hex(0x050505), 0); // 2% white background
    lv_obj_set_style_border_width(_chart, 0, 0);
    lv_obj_set_style_radius(_chart, 0, LV_PART_MAIN); // Explicitly set radius to 0 for sharp corners
    lv_obj_set_style_line_color(_chart, lv_color_hex(0x1A1A1A), LV_PART_MAIN); // Grid line color (10% white)
    // Remove padding from the chart itself to use full area
    lv_obj_set_style_pad_all(_chart, 0, LV_PART_MAIN);

    _series = lv_chart_add_series(_chart, lv_color_hex(0x2980b9), LV_CHART_AXIS_PRIMARY_Y);
    if (!_series) {
        Serial.println("[LineGraphRenderer-ERROR] Failed to create chart series.");
        lv_obj_del(_chart); // Clean up chart if series fails
        _chart = nullptr;
        return;
    }

    lv_obj_set_style_size(_chart, 0, 0, LV_PART_INDICATOR); // No indicators (dots on points)
    lv_obj_set_style_line_width(_chart, 2, LV_PART_ITEMS); // Line width for the series

    // Initial refresh of the chart might be good if it has default state
    // lv_chart_refresh(_chart);
    // InsightCard will do a global refresh after calling createElements if needed.
}

void LineGraphRenderer::updateDisplay(InsightParser& parser, const String& title, const char* prefix, const char* suffix) {
    // Title is handled by InsightCard. This renderer updates the chart data.
    // prefix and suffix are ignored for LineGraphRenderer.
    size_t point_count = parser.getSeriesPointCount();
    if (point_count == 0) {
        // No data points, maybe clear the chart or show a message?
        // For now, clear existing points if any.
        dispatchToUI([this]() {
            if (isValidLVGLObject(_chart) && _series) {
                lv_chart_set_point_count(_chart, 0);
                lv_chart_refresh(_chart);
            }
        });
        return;
    }

    // Allocate memory for Y values from the parser
    // Using unique_ptr for automatic memory management.
    std::unique_ptr<double[]> y_values_from_parser(new double[point_count]);
    if (!parser.getSeriesYValues(y_values_from_parser.get())) {
        Serial.println("[LineGraphRenderer-ERROR] Failed to get Y series values from parser.");
        return;
    }

    // Find max value for scaling (logic from original InsightCard)
    double max_val = 0.0;
    if (point_count > 0) {
        max_val = y_values_from_parser[0];
        for (size_t i = 1; i < point_count; ++i) {
            if (y_values_from_parser[i] > max_val) max_val = y_values_from_parser[i];
        }
    }
    // Ensure max_val is not zero to avoid division by zero; if all values are <=0, chart range needs care.
    if (max_val <= 0) max_val = 1.0; // Default to 1 if all data is zero or negative to prevent scaling issues.

    double scale_factor = (max_val > 1000.0) ? (1000.0 / max_val) : 1.0;

    // Copy data for the UI thread: Capture y_values_from_parser by moving its contents to a shared_ptr.
    // This is because the lambda might outlive the current scope.
    // The parser itself is passed by reference but assumed to be valid during the lambda's queuing/execution
    // due to the shared_ptr in InsightCard::handleParsedData.
    
    // To pass the array safely to the lambda, we copy it into a structure that can be captured.
    // A std::vector is easier to capture by value for lambdas.
    std::vector<double> values_for_lambda(point_count);
    for(size_t i = 0; i < point_count; ++i) {
        values_for_lambda[i] = y_values_from_parser[i];
    }

    dispatchToUI([this, captured_values = std::move(values_for_lambda), point_count, max_val, scale_factor]() {
        if (!areElementsValid()) {
            Serial.println("[LineGraphRenderer-WARN] Chart/Series invalid in updateDisplay lambda.");
            return;
        }

        // Limit display points for performance/readability if desired, though original fix was to show all.
        // const size_t display_points = std::min(point_count, static_cast<size_t>(50)); // Example: Max 50 points
        const size_t display_points = point_count; // Show all points as per previous fixes

        lv_chart_set_point_count(_chart, display_points);
        // lv_chart_set_all_value(_chart, _series, 0); // Zero out before setting new values

        for (size_t i = 0; i < display_points; ++i) {
            int16_t y_val = static_cast<int16_t>(captured_values[i] * scale_factor);
            // LVGL chart y-values are typically positive. If your data can be negative,
            // you might need to adjust the range and how y_val is calculated.
            lv_chart_set_value_by_id(_chart, _series, i, y_val);
        }

        // Set chart range dynamically
        // LVGL charts typically handle positive values. If your data min is negative or very different,
        // the Y-axis range (lv_chart_set_range) needs to be set carefully.
        // For simplicity, assuming positive values and scaling towards a max of ~1000 * 1.1 on chart.
        lv_chart_set_range(_chart, LV_CHART_AXIS_PRIMARY_Y, 0, static_cast<int32_t>(max_val * scale_factor * 1.1));
        
        lv_chart_refresh(_chart);

        // Optional: Force a global display refresh if needed, though InsightCard might handle it.
        // lv_display_t* disp = lv_display_get_default();
        // if (disp) { lv_refr_now(disp); }
    }, true); // Send to front to prioritize data update rendering
}

void LineGraphRenderer::clearElements() {
    // Expected to be called from LVGL UI thread.
    if (isValidLVGLObject(_chart)) {
        lv_obj_del(_chart); // This also deletes series associated with the chart
    }
    _chart = nullptr;
    _series = nullptr; // Series is owned by chart, but good to nullify pointer.
}

bool LineGraphRenderer::areElementsValid() const {
    // Can be called from any thread.
    return isValidLVGLObject(_chart) && _series; // Series validity is tied to chart, but check both for clarity.
} 