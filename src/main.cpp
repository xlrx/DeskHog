#include <Arduino.h>
#include <lvgl.h>
#include <Adafruit_ST7789.h>
#include <Bounce2.h>

// Display dimensions
#define SCREEN_WIDTH 240
#define SCREEN_HEIGHT 135

// Button pins for ESP32-S3 Reverse TFT Feather
#define BUTTON_0 0  // BOOT button - pulled HIGH by default, LOW when pressed
#define BUTTON_1 1  // I2C power button - pulled LOW by default, HIGH when pressed
#define BUTTON_2 2  // User button - pulled LOW by default, HIGH when pressed

#define NUM_BUTTONS 3
#define NUM_CARDS 10

// LVGL display buffer size
#define LVGL_BUFFER_ROWS 135  // Full screen height

// Animation duration
#define PIP_ANIM_DURATION 150  // ms

// Global objects
Adafruit_ST7789 tft = Adafruit_ST7789(&SPI, TFT_CS, TFT_DC, TFT_RST);
SemaphoreHandle_t lvgl_mutex = NULL;

// LVGL objects
lv_obj_t *main_container;
lv_obj_t *scroll_indicator;  // New scroll indicator container
static const lv_coord_t card_height = SCREEN_HEIGHT;  // Make cards full screen height
static const lv_coord_t pip_size = 5;  // Normal pip size
static const lv_coord_t pip_size_active = 10;  // Active pip size

// LVGL display buffers
static lv_disp_draw_buf_t draw_buf;
static lv_color_t buf1[SCREEN_WIDTH * LVGL_BUFFER_ROWS];
static lv_color_t buf2[SCREEN_WIDTH * LVGL_BUFFER_ROWS];
static lv_disp_drv_t disp_drv;

// Button objects
Bounce2::Button buttons[NUM_BUTTONS];

// Card colors - darker variants for better contrast
static const lv_color_t card_colors[] = {
    lv_color_hex(0xC41E3A),  // Dark Red
    lv_color_hex(0x228B22),  // Forest Green
    lv_color_hex(0x00008B),  // Dark Blue
    lv_color_hex(0xB8860B),  // Dark Golden
    lv_color_hex(0x8B008B),  // Dark Magenta
    lv_color_hex(0x008B8B),  // Dark Cyan
    lv_color_hex(0xD2691E),  // Chocolate
    lv_color_hex(0x483D8B),  // Dark Slate Blue
    lv_color_hex(0x2F4F4F),  // Dark Slate Gray
    lv_color_hex(0x800000),  // Maroon
};

// Scroll event callback
static void scroll_event_cb(lv_event_t * e) {
    lv_obj_t * cont = lv_event_get_target(e);
    lv_area_t cont_a;
    lv_obj_get_coords(cont, &cont_a);
    int32_t cont_y_center = cont_a.y1 + lv_area_get_height(&cont_a) / 2;

    uint32_t child_cnt = lv_obj_get_child_cnt(cont);
    
    for(uint32_t i = 0; i < child_cnt; i++) {
        lv_obj_t * child = lv_obj_get_child(cont, i);
        // Remove all opacity effects - everything is fully opaque
        lv_obj_set_style_opa(child, LV_OPA_COVER, 0);
    }
}

// LVGL display flush callback
void my_disp_flush(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p) {
    uint32_t w = (area->x2 - area->x1 + 1);
    uint32_t h = (area->y2 - area->y1 + 1);
    
    tft.startWrite();
    tft.setAddrWindow(area->x1, area->y1, w, h);
    tft.writePixels((uint16_t*)color_p, w * h);
    tft.endWrite();
    
    lv_disp_flush_ready(disp);
}

// LVGL tick task - runs on core 1
static void lv_tick_task(void *arg) {
    (void)arg;
    while (1) {
        lv_tick_inc(10);
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

// LVGL update task - runs on core 1
void lvglTask(void *parameter) {
    while (1) {
        if (xSemaphoreTake(lvgl_mutex, portMAX_DELAY) == pdTRUE) {
            lv_timer_handler();
            xSemaphoreGive(lvgl_mutex);
        }
        
        vTaskDelay(pdMS_TO_TICKS(5));
    }
}

// Update scroll indicator
static void update_scroll_indicator(int active_index) {
    static int previous_index = 0;  // Keep track of previous active pip
    
    if (previous_index != active_index) {
        // Animate previous pip to shrink
        lv_obj_t * prev_pip = lv_obj_get_child(scroll_indicator, previous_index);
        lv_anim_t a;
        lv_anim_init(&a);
        lv_anim_set_var(&a, prev_pip);
        lv_anim_set_time(&a, PIP_ANIM_DURATION);
        lv_anim_set_values(&a, pip_size_active, pip_size);
        lv_anim_set_exec_cb(&a, (lv_anim_exec_xcb_t)lv_obj_set_width);
        lv_obj_set_style_bg_color(prev_pip, lv_color_hex(0x808080), 0);
        lv_anim_start(&a);

        lv_anim_t a2;
        lv_anim_init(&a2);
        lv_anim_set_var(&a2, prev_pip);
        lv_anim_set_time(&a2, PIP_ANIM_DURATION);
        lv_anim_set_values(&a2, pip_size_active, pip_size);
        lv_anim_set_exec_cb(&a2, (lv_anim_exec_xcb_t)lv_obj_set_height);
        lv_anim_start(&a2);

        // Animate new pip to grow
        lv_obj_t * new_pip = lv_obj_get_child(scroll_indicator, active_index);
        lv_anim_t b;
        lv_anim_init(&b);
        lv_anim_set_var(&b, new_pip);
        lv_anim_set_time(&b, PIP_ANIM_DURATION);
        lv_anim_set_values(&b, pip_size, pip_size_active);
        lv_anim_set_exec_cb(&b, (lv_anim_exec_xcb_t)lv_obj_set_width);
        lv_obj_set_style_bg_color(new_pip, lv_color_white(), 0);
        lv_anim_start(&b);

        lv_anim_t b2;
        lv_anim_init(&b2);
        lv_anim_set_var(&b2, new_pip);
        lv_anim_set_time(&b2, PIP_ANIM_DURATION);
        lv_anim_set_values(&b2, pip_size, pip_size_active);
        lv_anim_set_exec_cb(&b2, (lv_anim_exec_xcb_t)lv_obj_set_height);
        lv_anim_start(&b2);

        previous_index = active_index;
    }
}

// Button handling task - runs on core 0
void buttonTask(void *parameter) {
    static int current_card = 0;
    lv_obj_t * target_card = NULL;

    while (1) {
        // Update all buttons
        for (int i = 0; i < NUM_BUTTONS; i++) {
            buttons[i].update();
            
            if (buttons[i].pressed()) {
                if (xSemaphoreTake(lvgl_mutex, portMAX_DELAY) == pdTRUE) {
                    // Next card (Button 0)
                    if (i == 0) {
                        current_card = (current_card < NUM_CARDS - 1) ? current_card + 1 : 0;
                    }
                    // Previous card (Button 2)
                    else if (i == 2) {
                        current_card = (current_card > 0) ? current_card - 1 : NUM_CARDS - 1;
                    }
                    
                    target_card = lv_obj_get_child(main_container, current_card);
                    lv_obj_scroll_to_view(target_card, LV_ANIM_ON);
                    update_scroll_indicator(current_card);
                    
                    xSemaphoreGive(lvgl_mutex);
                }
            }
        }

        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

void setup() {
    Serial.begin(115200);
    
    // Initialize buttons
    buttons[0].attach(BUTTON_0, INPUT_PULLUP);
    buttons[0].interval(50);
    buttons[0].setPressedState(HIGH);
    
    buttons[1].attach(BUTTON_1, INPUT);
    buttons[1].interval(50);
    buttons[1].setPressedState(LOW);
    
    buttons[2].attach(BUTTON_2, INPUT);
    buttons[2].interval(50);
    buttons[2].setPressedState(LOW);

    // Create LVGL mutex
    lvgl_mutex = xSemaphoreCreateMutex();
    if (lvgl_mutex == NULL) {
        Serial.println("Could not create mutex");
        while (1);
    }

    // Initialize display
    SPI.begin();
    tft.init(SCREEN_HEIGHT, SCREEN_WIDTH);
    
    tft.setRotation(1);
    pinMode(TFT_BACKLITE, OUTPUT);
    digitalWrite(TFT_BACKLITE, HIGH);
    tft.fillScreen(ST77XX_BLACK);

    // Initialize LVGL
    lv_init();

    // Initialize display buffer
    lv_disp_draw_buf_init(&draw_buf, buf1, buf2, SCREEN_WIDTH * LVGL_BUFFER_ROWS);

    // Initialize display driver
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = SCREEN_WIDTH;
    disp_drv.ver_res = SCREEN_HEIGHT;
    disp_drv.flush_cb = my_disp_flush;
    disp_drv.draw_buf = &draw_buf;
    lv_disp_drv_register(&disp_drv);

    // Set screen background to black
    lv_obj_set_style_bg_color(lv_scr_act(), lv_color_black(), 0);
    lv_obj_set_style_bg_opa(lv_scr_act(), LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(lv_scr_act(), 0, 0);

    // Create main container
    main_container = lv_obj_create(lv_scr_act());
    lv_obj_set_size(main_container, SCREEN_WIDTH - pip_size_active, SCREEN_HEIGHT);
    lv_obj_align(main_container, LV_ALIGN_LEFT_MID, 0, 0);
    lv_obj_set_style_bg_color(main_container, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(main_container, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(main_container, 0, 0);
    lv_obj_set_style_pad_left(main_container, 0, 0);
    lv_obj_set_style_pad_right(main_container, 0, 0);
    lv_obj_set_style_pad_top(main_container, 0, 0);
    lv_obj_set_style_pad_bottom(main_container, 0, 0);
    lv_obj_set_style_pad_row(main_container, 0, 0);  // No gap between cards
    lv_obj_set_flex_flow(main_container, LV_FLEX_FLOW_COLUMN);
    lv_obj_add_event_cb(main_container, scroll_event_cb, LV_EVENT_SCROLL, NULL);
    lv_obj_set_scroll_dir(main_container, LV_DIR_VER);
    lv_obj_set_scroll_snap_y(main_container, LV_SCROLL_SNAP_CENTER);
    lv_obj_set_scrollbar_mode(main_container, LV_SCROLLBAR_MODE_OFF);

    // Create scroll indicator container
    scroll_indicator = lv_obj_create(lv_scr_act());
    lv_obj_set_size(scroll_indicator, pip_size_active, SCREEN_HEIGHT);
    lv_obj_align(scroll_indicator, LV_ALIGN_RIGHT_MID, 0, 0);  // Align exactly to right edge
    lv_obj_set_style_bg_color(scroll_indicator, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(scroll_indicator, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(scroll_indicator, 0, 0);
    lv_obj_set_style_pad_all(scroll_indicator, 0, 0);
    lv_obj_set_style_pad_top(scroll_indicator, pip_size_active, 0);     // Add padding equal to pip size
    lv_obj_set_style_pad_bottom(scroll_indicator, pip_size_active, 0);  // Add padding equal to pip size
    lv_obj_set_flex_flow(scroll_indicator, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(scroll_indicator, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(scroll_indicator, LV_OBJ_FLAG_SCROLLABLE);     // Disable scrolling

    // Create indicator pips
    for(int i = 0; i < NUM_CARDS; i++) {
        lv_obj_t * pip = lv_obj_create(scroll_indicator);
        lv_obj_set_size(pip, pip_size, pip_size);
        lv_obj_set_style_radius(pip, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_color(pip, lv_color_hex(0x808080), 0);
        lv_obj_set_style_border_width(pip, 0, 0);
    }

    // Set first pip as active
    lv_obj_t * first_pip = lv_obj_get_child(scroll_indicator, 0);
    lv_obj_set_size(first_pip, pip_size_active, pip_size_active);
    lv_obj_set_style_bg_color(first_pip, lv_color_white(), 0);

    // Create cards
    for(int i = 0; i < NUM_CARDS; i++) {
        lv_obj_t * card = lv_obj_create(main_container);
        lv_obj_set_size(card, lv_pct(100), SCREEN_HEIGHT);  // Full width and height
        lv_obj_set_style_radius(card, 8, 0);
        lv_obj_set_style_border_width(card, 0, 0);
        lv_obj_set_style_bg_color(card, card_colors[i % 10], 0);
        lv_obj_set_style_pad_all(card, 0, 0);
        
        lv_obj_t * label = lv_label_create(card);
        lv_label_set_text_fmt(label, "Card %d", i + 1);
        lv_obj_set_style_text_font(label, &lv_font_montserrat_18, 0);
        lv_obj_center(label);
        lv_obj_set_style_text_color(label, lv_color_white(), 0);
    }

    // Make sure the first card is centered
    lv_obj_scroll_to_view(lv_obj_get_child(main_container, 0), LV_ANIM_OFF);

    // Create LVGL tick task
    xTaskCreatePinnedToCore(
        lv_tick_task,
        "lv_tick_task",
        2048,
        NULL,
        2,
        NULL,
        1
    );

    // Create LVGL handler task
    xTaskCreatePinnedToCore(
        lvglTask,
        "lvglTask",
        4096,
        NULL,
        2,
        NULL,
        1
    );

    // Create button handler task
    xTaskCreatePinnedToCore(
        buttonTask,
        "buttonTask",
        2048,
        NULL,
        1,
        NULL,
        0
    );
}

void loop() {
    // Empty - tasks handle everything
    vTaskDelete(NULL);
}