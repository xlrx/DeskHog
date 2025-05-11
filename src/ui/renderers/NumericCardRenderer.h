#ifndef NUMERIC_CARD_RENDERER_H
#define NUMERIC_CARD_RENDERER_H

#include "InsightRendererBase.h"
#include "../Style.h" // Assuming Style.h is in src/ui/
#include "NumberFormat.h" // Corrected path based on file search

class NumericCardRenderer : public InsightRendererBase {
public:
    NumericCardRenderer();
    ~NumericCardRenderer() override;

    void createElements(lv_obj_t* parent_container) override;
    void updateDisplay(InsightParser& parser, const String& title) override;
    void clearElements() override;
    bool areElementsValid() const override;

private:
    lv_obj_t* _value_label; // LVGL label object for displaying the numeric value
    // Title is handled by InsightCard itself, this renderer only cares about the value display.

    // Helper to format numbers (can be static or instance method)
    // If NumberFormat utility is comprehensive, this might not be needed here.
    // For now, assuming a local helper or direct use of NumberFormat.
    void formatNumericValue(double value, char* buffer, size_t buffer_size);
};

#endif // NUMERIC_CARD_RENDERER_H 