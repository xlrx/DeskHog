#ifndef FUNNEL_RENDERER_H
#define FUNNEL_RENDERER_H

#include "InsightRendererBase.h"
#include "../Style.h" // For styles, colors, fonts
#include "NumberFormat.h" // Corrected path for number formatting
#include <vector> // May need for storing step data if complex

class FunnelRenderer : public InsightRendererBase {
public:
    FunnelRenderer();
    ~FunnelRenderer() override;

    void createElements(lv_obj_t* parent_container) override;
    void updateDisplay(InsightParser& parser, const String& title) override;
    void clearElements() override;
    bool areElementsValid() const override;

private:
    // Constants for funnel layout (previously in InsightCard)
    static constexpr int MAX_FUNNEL_STEPS = 5;     
    static constexpr int MAX_BREAKDOWNS = 5;       
    static constexpr int FUNNEL_BAR_HEIGHT = 5;    
    static constexpr int FUNNEL_BAR_GAP = 24;      
    // static constexpr int FUNNEL_LEFT_MARGIN = 0; // Might not be needed if aligning within container
    static constexpr int FUNNEL_LABEL_HEIGHT = 20; // Restored to original value, as 15 might be too small for Style::valueFont()

    lv_obj_t* _funnel_main_container; // A container created by this renderer within parent_container
    
    // Arrays for LVGL objects
    lv_obj_t* _funnel_step_bars[MAX_FUNNEL_STEPS];
    lv_obj_t* _funnel_step_labels[MAX_FUNNEL_STEPS];
    lv_obj_t* _funnel_bar_segments[MAX_FUNNEL_STEPS][MAX_BREAKDOWNS];
    lv_color_t _breakdown_colors[MAX_BREAKDOWNS];

    void initBreakdownColors();

    // Helper to reset all element pointers to nullptr
    void resetElementPointers();
};

#endif // FUNNEL_RENDERER_H 