#include "FunnelRenderer.h"
#include "NumberFormat.h"
#include <algorithm> // For std::min, std::max

FunnelRenderer::FunnelRenderer()
    : _funnel_main_container(nullptr) {
    resetElementPointers();
    initBreakdownColors(); // Initialize colors at construction
    // Serial.println("[FunnelRenderer] Constructor");
}

FunnelRenderer::~FunnelRenderer() {
    // Serial.println("[FunnelRenderer] Destructor");
    // Relies on InsightCard calling clearElements.
}

void FunnelRenderer::resetElementPointers() {
    _funnel_main_container = nullptr;
    for (int i = 0; i < MAX_FUNNEL_STEPS; ++i) {
        _funnel_step_bars[i] = nullptr;
        _funnel_step_labels[i] = nullptr;
        for (int j = 0; j < MAX_BREAKDOWNS; ++j) {
            _funnel_bar_segments[i][j] = nullptr;
        }
    }
}

void FunnelRenderer::initBreakdownColors() {
    _breakdown_colors[0] = lv_color_hex(0x2980b9);  // Blue
    _breakdown_colors[1] = lv_color_hex(0x27ae60);  // Green
    _breakdown_colors[2] = lv_color_hex(0x8e44ad);  // Purple
    _breakdown_colors[3] = lv_color_hex(0xd35400);  // Orange
    _breakdown_colors[4] = lv_color_hex(0xc0392b);  // Red
    // Ensure any unused slots get a default, though MAX_BREAKDOWNS should be respected
    for (int i = 5; i < MAX_BREAKDOWNS; ++i) { // Should not run if MAX_BREAKDOWNS is 5
        _breakdown_colors[i] = lv_color_hex(0x7f8c8d); // Gray for extra, if any
    }
}

void FunnelRenderer::createElements(lv_obj_t* parent_container) {
    if (!isValidLVGLObject(parent_container)) {
        Serial.println("[FunnelRenderer-ERROR] Parent container invalid in createElements.");
        return;
    }

    // Create a main container for all funnel elements within the parent_container
    // This allows easier clearing and management.
    _funnel_main_container = lv_obj_create(parent_container);
    if (!_funnel_main_container) {
        Serial.println("[FunnelRenderer-ERROR] Failed to create _funnel_main_container.");
        return;
    }
    lv_obj_set_size(_funnel_main_container, lv_pct(100), lv_pct(100));
    lv_obj_align(_funnel_main_container, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_clear_flag(_funnel_main_container, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_pad_all(_funnel_main_container, 0, 0);
    lv_obj_set_style_border_width(_funnel_main_container, 0, 0);
    lv_obj_set_style_bg_opa(_funnel_main_container, LV_OPA_0, 0); // Transparent background

    lv_coord_t available_width = lv_obj_get_content_width(_funnel_main_container);

    for (int i = 0; i < MAX_FUNNEL_STEPS; ++i) {
        // Create bar container (a simple object to hold segments)
        _funnel_step_bars[i] = lv_obj_create(_funnel_main_container);
        if (!_funnel_step_bars[i]) continue; // Error handling: skip if creation fails
        
        lv_obj_set_size(_funnel_step_bars[i], available_width, FUNNEL_BAR_HEIGHT);
        lv_obj_set_style_bg_opa(_funnel_step_bars[i], LV_OPA_0, 0); // Transparent bar container
        lv_obj_set_style_border_width(_funnel_step_bars[i], 0, 0);
        lv_obj_set_style_pad_all(_funnel_step_bars[i], 0, 0);
        lv_obj_clear_flag(_funnel_step_bars[i], LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_flag(_funnel_step_bars[i], LV_OBJ_FLAG_HIDDEN); // Initially hidden

        // Create label for the step
        _funnel_step_labels[i] = lv_label_create(_funnel_main_container);
        if (!_funnel_step_labels[i]) continue;

        lv_obj_set_style_text_color(_funnel_step_labels[i], Style::valueColor(), 0);
        lv_obj_set_style_text_font(_funnel_step_labels[i], Style::valueFont(), 0); // Smaller font for step labels
        lv_label_set_long_mode(_funnel_step_labels[i], LV_LABEL_LONG_DOT);
        lv_obj_set_width(_funnel_step_labels[i], available_width);
        lv_obj_set_height(_funnel_step_labels[i], FUNNEL_LABEL_HEIGHT);
        lv_obj_add_flag(_funnel_step_labels[i], LV_OBJ_FLAG_HIDDEN); // Initially hidden

        // Create segments within the bar container
        for (int j = 0; j < MAX_BREAKDOWNS; ++j) {
            _funnel_bar_segments[i][j] = lv_obj_create(_funnel_step_bars[i]);
            if (!_funnel_bar_segments[i][j]) continue;

            lv_obj_set_height(_funnel_bar_segments[i][j], FUNNEL_BAR_HEIGHT);
            lv_obj_set_style_bg_color(_funnel_bar_segments[i][j], _breakdown_colors[j], 0);
            lv_obj_set_style_border_width(_funnel_bar_segments[i][j], 0, 0);
            lv_obj_set_style_radius(_funnel_bar_segments[i][j], 0, 0);
            lv_obj_set_style_pad_all(_funnel_bar_segments[i][j], 0, 0);
            lv_obj_add_flag(_funnel_bar_segments[i][j], LV_OBJ_FLAG_HIDDEN); // Initially hidden
        }
    }
    // Serial.println("[FunnelRenderer] Funnel elements created successfully.");
}

void FunnelRenderer::updateDisplay(InsightParser& parser, const String& title_str) {
    Serial.printf("[FunnelRenderer] updateDisplay for title: %s\n", title_str.c_str()); // Verify this is called

    size_t raw_step_count = parser.getFunnelStepCount();
    size_t raw_breakdown_count = parser.getFunnelBreakdownCount();
    Serial.printf("[FunnelRenderer] Parser reports: step_count = %u, breakdown_count = %u\n",
                  (unsigned int)raw_step_count, (unsigned int)raw_breakdown_count);

    size_t step_count = std::min(raw_step_count, static_cast<size_t>(MAX_FUNNEL_STEPS));
    size_t breakdown_count = std::min(raw_breakdown_count, static_cast<size_t>(MAX_BREAKDOWNS));
    Serial.printf("[FunnelRenderer] Effective: step_count = %u, breakdown_count = %u\n",
                  (unsigned int)step_count, (unsigned int)breakdown_count);

    if (step_count == 0) {
        Serial.println("[FunnelRenderer] step_count is 0, hiding elements.");
        // No steps, clear display or show message
        dispatchToUI([this]() {
            if (!areElementsValid()) return;
            for (int i = 0; i < MAX_FUNNEL_STEPS; ++i) {
                if (isValidLVGLObject(_funnel_step_bars[i])) lv_obj_add_flag(_funnel_step_bars[i], LV_OBJ_FLAG_HIDDEN);
                if (isValidLVGLObject(_funnel_step_labels[i])) lv_obj_add_flag(_funnel_step_labels[i], LV_OBJ_FLAG_HIDDEN);
            }
        });
        return;
    }

    uint32_t step_counts_total[MAX_FUNNEL_STEPS] = {0};
    // Conversion rates not directly used for display here, but parser might need it.
    if (!parser.getFunnelTotalCounts(0, step_counts_total, nullptr)) {
        Serial.println("[FunnelRenderer-ERROR] Failed to get funnel total counts from parser.");
        return;
    }

    uint32_t total_first_step = step_counts_total[0];
    Serial.printf("[FunnelRenderer] total_first_step = %u\n", (unsigned int)total_first_step);
    if (total_first_step == 0 && step_count > 0) { // Allow processing if step_count is 0 (handled above)
        Serial.println("[FunnelRenderer-WARN] First funnel step count is zero. Funnel will appear empty or scaled strangely.");
        // To avoid division by zero later, if we must proceed, set to 1, but data is effectively empty.
        // total_first_step = 1; // Or, clear display as above.
    }

    // Prepare data structure for UI update lambda
    struct FunnelStepUIData {
        String label_text;
        float relative_width_to_first_step; // 0.0 - 1.0
        struct SegmentUIData {
            float width_pixels; 
            float offset_pixels; 
        } segments[MAX_BREAKDOWNS];
    };
    std::vector<FunnelStepUIData> ui_steps_data(step_count);

    lv_coord_t available_width_for_bars = lv_obj_get_content_width(_funnel_main_container);

    for (size_t i = 0; i < step_count; ++i) {
        FunnelStepUIData& current_ui_step = ui_steps_data[i];
        current_ui_step.relative_width_to_first_step = (total_first_step > 0) ? 
            static_cast<float>(step_counts_total[i]) / total_first_step : 0.0f;

        char step_name_buffer[64] = {0};
        parser.getFunnelStepData(0, i, step_name_buffer, sizeof(step_name_buffer), nullptr, nullptr, nullptr);
        
        char number_buffer[20];
        NumberFormat::addThousandsSeparators(number_buffer, sizeof(number_buffer), step_counts_total[i]); // Using global NumberFormat, corrected function name

        String label = String(number_buffer);
        if (step_name_buffer[0] != '\0') {
            label += " - ";
            label += step_name_buffer;
        }
        if (i > 0 && total_first_step > 0) {
            uint32_t percentage = (step_counts_total[i] * 100) / total_first_step;
            label += " (";
            label += String(percentage);
            label += "%)";
        }
        current_ui_step.label_text = label;

        // Calculate breakdown segments for this step
        uint32_t breakdown_val_counts[MAX_BREAKDOWNS] = {0};
        if (parser.getFunnelBreakdownComparison(i, breakdown_val_counts, nullptr) && step_counts_total[i] > 0) {
            float total_width_for_this_step_bar = available_width_for_bars * current_ui_step.relative_width_to_first_step;
            float current_offset = 0.0f;
            for (size_t j = 0; j < breakdown_count; ++j) {
                float segment_percentage_of_step = static_cast<float>(breakdown_val_counts[j]) / step_counts_total[i];
                float segment_width_pixels = total_width_for_this_step_bar * segment_percentage_of_step;
                
                current_ui_step.segments[j].width_pixels = segment_width_pixels;
                current_ui_step.segments[j].offset_pixels = current_offset;
                current_offset += segment_width_pixels;
            }
        }
    }

    // Dispatch UI update
    dispatchToUI([this, captured_steps_data = std::move(ui_steps_data), step_count, breakdown_count, available_width_for_bars]() {
        if (!areElementsValid()) {
            Serial.println("[FunnelRenderer-WARN] Funnel elements invalid in updateDisplay lambda.");
            return;
        }

        int y_offset = 0;
        for (size_t i = 0; i < step_count; ++i) {
            const auto& step_data = captured_steps_data[i];

            if (isValidLVGLObject(_funnel_step_bars[i])) {
                lv_obj_clear_flag(_funnel_step_bars[i], LV_OBJ_FLAG_HIDDEN);
                lv_obj_align(_funnel_step_bars[i], LV_ALIGN_TOP_LEFT, 0, y_offset);
                // Ensure the bar container is set to the full available width before placing segments
                lv_obj_set_width(_funnel_step_bars[i], available_width_for_bars); 

                for (size_t j = 0; j < breakdown_count; ++j) {
                    if (isValidLVGLObject(_funnel_bar_segments[i][j])) {
                        int seg_width = static_cast<int>(step_data.segments[j].width_pixels);
                        // Ensure visible segments have at least 1px width if they have any data
                        if (seg_width == 0 && step_data.segments[j].width_pixels > 0) seg_width = 1;

                        if (seg_width > 0) {
                            lv_obj_set_size(_funnel_bar_segments[i][j], seg_width, FUNNEL_BAR_HEIGHT);
                            lv_obj_align(_funnel_bar_segments[i][j], LV_ALIGN_LEFT_MID, static_cast<int>(step_data.segments[j].offset_pixels), 0);
                            lv_obj_clear_flag(_funnel_bar_segments[i][j], LV_OBJ_FLAG_HIDDEN);
                        } else {
                            lv_obj_add_flag(_funnel_bar_segments[i][j], LV_OBJ_FLAG_HIDDEN);
                        }
                    }
                }
                // Hide unused segments for this step
                for (size_t j = breakdown_count; j < MAX_BREAKDOWNS; ++j) {
                     if (isValidLVGLObject(_funnel_bar_segments[i][j])) {
                        lv_obj_add_flag(_funnel_bar_segments[i][j], LV_OBJ_FLAG_HIDDEN);
                    }
                }
            }

            if (isValidLVGLObject(_funnel_step_labels[i])) {
                lv_label_set_text(_funnel_step_labels[i], step_data.label_text.c_str());
                lv_obj_clear_flag(_funnel_step_labels[i], LV_OBJ_FLAG_HIDDEN);
                lv_obj_align(_funnel_step_labels[i], LV_ALIGN_TOP_LEFT, 1, y_offset + FUNNEL_BAR_HEIGHT + 2); // +2 for small gap
            }
            y_offset += FUNNEL_BAR_HEIGHT + FUNNEL_BAR_GAP;
        }

        // Hide unused steps (bars and labels)
        for (size_t i = step_count; i < MAX_FUNNEL_STEPS; ++i) {
            if (isValidLVGLObject(_funnel_step_bars[i])) lv_obj_add_flag(_funnel_step_bars[i], LV_OBJ_FLAG_HIDDEN);
            if (isValidLVGLObject(_funnel_step_labels[i])) lv_obj_add_flag(_funnel_step_labels[i], LV_OBJ_FLAG_HIDDEN);
        }
        
        // Optional: force refresh
        // lv_obj_invalidate(_funnel_main_container); // Invalidate the specific container
        // lv_display_t* disp = lv_display_get_default();
        // if (disp) { lv_refr_now(disp); }

    }, true); // Send to front for responsiveness
}

void FunnelRenderer::clearElements() {
    // Expected to be called from LVGL UI thread.
    // Serial.println("[FunnelRenderer] Clearing elements.");
    if (isValidLVGLObject(_funnel_main_container)) {
        lv_obj_del(_funnel_main_container); // Deletes main container and all its children
    }
    // All pointers to children are now invalid and should be nullified.
    resetElementPointers();
}

bool FunnelRenderer::areElementsValid() const {
    // A basic check. If main container is valid, assume its children were created (or are correctly null).
    // More thorough checks could iterate all element pointers if needed.
    return isValidLVGLObject(_funnel_main_container);
} 