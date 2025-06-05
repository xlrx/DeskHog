#ifndef LINE_GRAPH_RENDERER_H
#define LINE_GRAPH_RENDERER_H

#include "InsightRendererBase.h"
#include "../Style.h" // For styles, colors, fonts
// NumberFormat might not be directly needed here if data comes pre-formatted or scaling is internal

class LineGraphRenderer : public InsightRendererBase {
public:
    LineGraphRenderer();
    ~LineGraphRenderer() override;

    void createElements(lv_obj_t* parent_container) override;
    void updateDisplay(InsightParser& parser, const String& title, const char* prefix = nullptr, const char* suffix = nullptr) override;
    void clearElements() override;
    bool areElementsValid() const override;

private:
    lv_obj_t* _chart;           // LVGL chart object
    lv_chart_series_t* _series; // LVGL chart series object

    // Constants for chart appearance - can be defined here or moved to Style.h if more global
    // For now, keeping them local to the renderer.
    static constexpr int DEFAULT_GRAPH_WIDTH = 230;  // Example, adjust as needed
    static constexpr int DEFAULT_GRAPH_HEIGHT = 90; // Example, adjust as needed
    // These might be determined by parent_container size in createElements instead.
};

#endif // LINE_GRAPH_RENDERER_H 