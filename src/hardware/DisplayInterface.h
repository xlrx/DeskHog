#ifndef LVGL_DISPLAY_MANAGER_H
#define LVGL_DISPLAY_MANAGER_H

#include <Arduino.h>
#include <lvgl.h>
#include <Adafruit_ST7789.h>
#include <SPI.h>
#include "esp_heap_caps.h"

class DisplayInterface {
public:
    // Constructor
    DisplayInterface(
        uint16_t screen_width,
        uint16_t screen_height,
        uint16_t buffer_rows,
        int8_t cs_pin,
        int8_t dc_pin,
        int8_t rst_pin,
        int8_t backlight_pin
    );
    
    // Destructor
    ~DisplayInterface();
    
    // Initialize display and LVGL
    void begin();
    
    // Get the display object
    Adafruit_ST7789* getDisplay();
    
    // Run LVGL tasks (to be called in a task)
    void handleLVGLTasks();
    
    // LVGL tick handler (to be called in a task)
    static void tickTask(void* arg);
    
    // Take mutex
    bool takeMutex(TickType_t timeout = portMAX_DELAY);
    
    // Give mutex
    void giveMutex();
    
    // Get mutex pointer
    SemaphoreHandle_t* getMutexPtr();
    
private:
    // Display and buffer parameters
    uint16_t _screen_width;
    uint16_t _screen_height;
    uint16_t _buffer_rows;
    int8_t _cs_pin;
    int8_t _dc_pin;
    int8_t _rst_pin;
    int8_t _backlight_pin;
    
    // Global objects
    Adafruit_ST7789* _tft;
    SemaphoreHandle_t _lvgl_mutex;
    
    // LVGL display buffers
    lv_disp_draw_buf_t _draw_buf;
    lv_color_t* _buf1;
    lv_color_t* _buf2;
    lv_disp_drv_t _disp_drv;
    
    // Display flush callback
    static void _disp_flush(lv_disp_drv_t* disp, const lv_area_t* area, lv_color_t* color_p);
};

#endif // LVGL_DISPLAY_MANAGER_H