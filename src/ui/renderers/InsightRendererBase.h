#ifndef INSIGHT_RENDERER_BASE_H
#define INSIGHT_RENDERER_BASE_H

#include "lvgl.h"
#include "../../posthog/parsers/InsightParser.h" // Adjusted path
#include <Arduino.h> // For String, if used in titles or other data
#include <functional> // For std::function

// Forward declaration if InsightCard needs to be referenced, though ideally, it shouldn't be directly.
// class InsightCard; 

// It's tricky to include InsightCard.h here if InsightCard.h includes this file
// or specific renderer headers. For the static dispatch method, we need InsightCard.
// A forward declaration won't work for calling a static method.
// Solution: Ensure InsightCard.h does NOT include any specific renderer headers,
// only InsightRendererBase.h if necessary (e.g. for the std::unique_ptr type).
// Then, InsightRendererBase.h can safely include InsightCard.h for the static dispatcher.
#include "../InsightCard.h" // Adjusted path
// Note: If InsightCard.h needs to include this for unique_ptr<InsightRendererBase>,
// that's fine as long as InsightCard.h doesn't also include concrete renderer headers.
// The UICallback is now self-contained or in its own header, not needed here directly
// if InsightCard::dispatchToLVGLTask handles UICallback creation.

class InsightRendererBase {
public:
    virtual ~InsightRendererBase() = default;

    /**
     * @brief Creates the specific UI elements for this insight type.
     * This method will be called on the LVGL UI thread.
     * 
     * @param parent_container The LVGL object within which to create the elements.
     * @param initial_title The initial title to set for the insight.
     */
    virtual void createElements(lv_obj_t* parent_container) = 0;

    /**
     * @brief Updates the display with new data from the parser.
     * This method will be called when new data for the insight is received.
     * The renderer is responsible for dispatching its internal LVGL calls to the UI thread.
     * 
     * @param parser The InsightParser instance containing the new data.
     * @param title The title of the insight.
     */
    virtual void updateDisplay(InsightParser& parser, const String& title) = 0;

    /**
     * @brief Clears/deletes all UI elements created by this renderer.
     * This method will be called on the LVGL UI thread before this renderer is destroyed
     * or when the insight type changes.
     */
    virtual void clearElements() = 0;

    /**
     * @brief Checks if the core UI elements managed by this renderer are valid.
     * Useful for pre-update validation.
     * @return true if elements are valid, false otherwise.
     */
    virtual bool areElementsValid() const = 0;

protected:
    // Helper to dispatch UI updates to the LVGL task using InsightCard's static method
    static void dispatchToUI(std::function<void()> func, bool to_front = false) {
        // Now calls the static method from InsightCard
        InsightCard::dispatchToLVGLTask(std::move(func), to_front);
    }

    // Helper to check LVGL object validity (can be used by derived classes)
    static bool isValidLVGLObject(lv_obj_t* obj) {
        return obj && lv_obj_is_valid(obj);
    }
};

#endif // INSIGHT_RENDERER_BASE_H 