#include <Arduino.h>
#include <lvgl.h>
#include <Adafruit_ST7789.h>
#include <Bounce2.h>

// Display dimensions
#define SCREEN_WIDTH 240
#define SCREEN_HEIGHT 135

// Button pins for ESP32-S3 Reverse TFT Feather
#define BUTTON_A 0  // BOOT button - pulled HIGH by default, LOW when pressed
#define BUTTON_B 1  // I2C power button - pulled LOW by default, HIGH when pressed
#define BUTTON_C 2  // User button - pulled LOW by default, HIGH when pressed

#define NUM_BUTTONS 3

// LVGL display buffer size
#define LVGL_BUFFER_ROWS 40  // Number of rows in the buffer

// Global objects
Adafruit_ST7789 tft = Adafruit_ST7789(&SPI, TFT_CS, TFT_DC, TFT_RST);
lv_obj_t *label;  // LVGL label object
volatile int counter = 0;  // Counter for button presses
SemaphoreHandle_t lvgl_mutex = NULL;

// LVGL display buffers
static lv_disp_draw_buf_t draw_buf;
static lv_color_t buf1[SCREEN_WIDTH * LVGL_BUFFER_ROWS];
static lv_color_t buf2[SCREEN_WIDTH * LVGL_BUFFER_ROWS];
static lv_disp_drv_t disp_drv;

// Button objects
Bounce2::Button buttons[NUM_BUTTONS];

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
        lv_tick_inc(5);
        vTaskDelay(pdMS_TO_TICKS(5));
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

// Button handling task - runs on core 0
void buttonTask(void *parameter) {
    char buf[128];  // Increased from 32 to 128 to accommodate longer text
    
    while (1) {
        bool updated = false;

        // Update all buttons
        for (int i = 0; i < NUM_BUTTONS; i++) {
            buttons[i].update();
            
            if (buttons[i].pressed()) {
                switch (i) {
                    case 0: // Button A
                        counter++;
                        break;
                    case 1: // Button B
                        counter--;
                        break;
                    case 2: // Button C
                        counter = 0;
                        break;
                }
                updated = true;
            }
        }

        if (updated && xSemaphoreTake(lvgl_mutex, portMAX_DELAY) == pdTRUE) {
            snprintf(buf, sizeof(buf), "Counter this is very long, bam, let's wrap this text, show me: %d", counter);
            lv_label_set_text(label, buf);
            xSemaphoreGive(lvgl_mutex);
        }

        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

void setup() {
    Serial.begin(115200);
    
    // Initialize buttons
    buttons[0].attach(BUTTON_A, INPUT_PULLUP);  // Button A - active LOW
    buttons[0].interval(50);  // Debounce interval in ms
    buttons[0].setPressedState(HIGH);  // Button A is active LOW
    
    buttons[1].attach(BUTTON_B, INPUT);  // Button B - active HIGH
    buttons[1].interval(50);
    buttons[1].setPressedState(LOW);
    
    buttons[2].attach(BUTTON_C, INPUT);  // Button C - active HIGH
    buttons[2].interval(50);
    buttons[2].setPressedState(LOW);

    // Initialize display
    tft.init(SCREEN_HEIGHT, SCREEN_WIDTH); // Note: swapped for rotation
    tft.setRotation(1);
    pinMode(TFT_BACKLITE, OUTPUT);
    digitalWrite(TFT_BACKLITE, HIGH);

    // Create mutex for LVGL
    lvgl_mutex = xSemaphoreCreateMutex();
    if (lvgl_mutex == NULL) {
        Serial.println("Could not create mutex");
        while (1);
    }

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

    // Create a label on the display
    label = lv_label_create(lv_scr_act());
    
    // Set larger font
    lv_obj_set_style_text_font(label, &lv_font_montserrat_18, 0);
    
    // Enable text wrapping
    lv_label_set_long_mode(label, LV_LABEL_LONG_WRAP);
    // Set width to screen width minus margins
    lv_obj_set_width(label, SCREEN_WIDTH - 20);  // 10px margin on each side
    
    lv_label_set_text(label, "Press any button!");
    lv_obj_align(label, LV_ALIGN_CENTER, 0, 0);

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