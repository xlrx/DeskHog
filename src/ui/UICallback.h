#ifndef UI_CALLBACK_H
#define UI_CALLBACK_H

#include <functional>

// Helper class for memory-safe UI callbacks, typically for LVGL operations
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

#endif // UI_CALLBACK_H 