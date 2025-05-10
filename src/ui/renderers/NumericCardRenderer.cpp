#include "NumericCardRenderer.h"
#include "NumberFormat.h"

NumericCardRenderer::NumericCardRenderer()
    : _value_label(nullptr) {
    // Serial.println("[NumericRenderer] Constructor");
}

NumericCardRenderer::~NumericCardRenderer() {
    // Serial.println("[NumericRenderer] Destructor, ensuring elements are cleared.");
    // clearElements is usually called by InsightCard before renderer is destroyed,
    // but an extra check or call here (if dispatched to UI thread) could be a safeguard.
    // For now, relying on InsightCard's explicit call to clearElements.
    // If _value_label is not null, it means clearElements wasn't called or didn't complete,
    // which would be a bug in the management logic.
    if (_value_label) {
        // This would be problematic if called from a non-LVGL thread.
        // Consider if a final cleanup dispatch is needed here, though it adds complexity.
        // Serial.println("[NumericRenderer-WARN] Destructor called with non-null _value_label. Potential leak or improper cleanup.");
    }
}

void NumericCardRenderer::createElements(lv_obj_t* parent_container) {
    // This method is expected to be called from the LVGL UI thread.
    if (!isValidLVGLObject(parent_container)) {
        Serial.println("[NumericRenderer-ERROR] Parent container is invalid in createElements.");
        return;
    }

    // Serial.printf("[NumericRenderer] Creating elements in container %p. Core: %d\n", parent_container, xPortGetCoreID());

    _value_label = lv_label_create(parent_container);
    if (!_value_label) {
        Serial.println("[NumericRenderer-ERROR] Failed to create value label.");
        return;
    }
    lv_obj_center(_value_label); // Center it in the parent_container
    lv_obj_set_style_text_font(_value_label, Style::largeValueFont(), 0);
    lv_obj_set_style_text_color(_value_label, Style::valueColor(), 0);
    lv_label_set_text(_value_label, "..."); // Initial placeholder text
}

void NumericCardRenderer::updateDisplay(InsightParser& parser, const String& title) {
    // Title is handled by InsightCard, we only update the value label here.
    double value = parser.getNumericCardValue();

    // Data processing (getting value) is done here.
    // LVGL operations are dispatched to the UI thread.
    dispatchToUI([this, value]() {
        // Serial.printf("[NumericRenderer] Updating display on UI thread. Label: %p, Core: %d\n", _value_label, xPortGetCoreID());
        if (isValidLVGLObject(_value_label)) {
            char value_buffer[32];
            formatNumericValue(value, value_buffer, sizeof(value_buffer));
            lv_label_set_text(_value_label, value_buffer);
        } else {
            Serial.println("[NumericRenderer-WARN] _value_label invalid in updateDisplay lambda.");
        }
        // Optional: force refresh if this specific update needs it immediately.
        // lv_display_t* disp = lv_display_get_default();
        // if (disp) { lv_refr_now(disp); }
    });
}

void NumericCardRenderer::clearElements() {
    // This method is expected to be called from the LVGL UI thread.
    // Serial.printf("[NumericRenderer] Clearing elements. Label: %p. Core: %d\n", _value_label, xPortGetCoreID());
    if (isValidLVGLObject(_value_label)) {
        lv_obj_del(_value_label); // lv_obj_del is safe if obj is already deleted or null
    }
    _value_label = nullptr; // Ensure pointer is nullified
}

bool NumericCardRenderer::areElementsValid() const {
    // This can be called from any thread.
    return isValidLVGLObject(_value_label);
}

// Private helper for number formatting
// This is a direct adaptation of the original InsightCard::formatNumber
void NumericCardRenderer::formatNumericValue(double value, char* buffer, size_t buffer_size) {
    if (buffer_size == 0) return;
    buffer[0] = '\0'; // Ensure null termination for safety

    if (value >= 1000000.0) {
        snprintf(buffer, buffer_size, "%.1fM", value / 1000000.0);
    } else if (value >= 0 && value < 1000000.0 && value == static_cast<uint32_t>(value)) {
        // For whole numbers less than 1,000,000 and non-negative
        NumberFormat::addThousandsSeparators(buffer, buffer_size, static_cast<uint32_t>(value));
    } else if (value >= 1000.0 && value < 1000000.0) { // For non-whole numbers between 1k and 1M, show K
        snprintf(buffer, buffer_size, "%.1fK", value / 1000.0);
    } else if (value < 1000.0 && value >= 0) { // For positive numbers less than 1000 (potentially with decimals)
         snprintf(buffer, buffer_size, "%.1f", value); // Show one decimal for consistency, or use "%.0f" for whole numbers
    } else if (value < 0 && value > -1000.0) { // For negative numbers greater than -1000
        snprintf(buffer, buffer_size, "%.1f", value);
    } else if (value <= -1000.0 && value > -1000000.0) { // For negative numbers between -1k and -1M, show K
        snprintf(buffer, buffer_size, "%.1fK", value / 1000.0);
    } else { // General case for other numbers (e.g. very small, very large negative)
        snprintf(buffer, buffer_size, "%.1f", value);
    }
    // Ensure buffer is always null-terminated, snprintf should do this if space allows.
    buffer[buffer_size - 1] = '\0';
} 