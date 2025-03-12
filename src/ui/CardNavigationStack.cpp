#include "CardNavigationStack.h"

// Static instance pointer
CardNavigationStack* CardNavigationStack::_instance = nullptr;

// External button objects - defined in main.cpp
extern Bounce2::Button buttons[];

// Use the same NUM_BUTTONS definition as main.cpp
#define NUM_BUTTONS 3

CardNavigationStack::CardNavigationStack(lv_obj_t* parent, uint16_t width, uint16_t height, uint8_t num_cards)
    : _parent(parent), _width(width), _height(height), _num_cards(num_cards), _current_card(0), _mutex_ptr(nullptr) {
    
    // Store instance for static task
    _instance = this;
    
    // Create main container
    _main_container = lv_obj_create(_parent);
    lv_obj_set_size(_main_container, _width - PIP_SIZE_ACTIVE, _height);
    lv_obj_align(_main_container, LV_ALIGN_LEFT_MID, 0, 0);
    lv_obj_set_style_bg_color(_main_container, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(_main_container, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(_main_container, 0, 0);
    lv_obj_set_style_pad_left(_main_container, 0, 0);
    lv_obj_set_style_pad_right(_main_container, 0, 0);
    lv_obj_set_style_pad_top(_main_container, 0, 0);
    lv_obj_set_style_pad_bottom(_main_container, 0, 0);
    lv_obj_set_style_pad_row(_main_container, 0, 0);  // No gap between cards
    lv_obj_set_flex_flow(_main_container, LV_FLEX_FLOW_COLUMN);
    lv_obj_add_event_cb(_main_container, _scroll_event_cb, LV_EVENT_SCROLL, NULL);
    lv_obj_set_scroll_dir(_main_container, LV_DIR_VER);
    lv_obj_set_scroll_snap_y(_main_container, LV_SCROLL_SNAP_CENTER);
    lv_obj_set_scrollbar_mode(_main_container, LV_SCROLLBAR_MODE_OFF);

    // Create scroll indicator container
    _scroll_indicator = lv_obj_create(_parent);
    lv_obj_set_size(_scroll_indicator, PIP_SIZE_ACTIVE, _height);
    lv_obj_align(_scroll_indicator, LV_ALIGN_RIGHT_MID, 0, 0);  // Align exactly to right edge
    lv_obj_set_style_bg_color(_scroll_indicator, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(_scroll_indicator, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(_scroll_indicator, 0, 0);
    lv_obj_set_style_pad_all(_scroll_indicator, 0, 0);
    lv_obj_set_style_pad_top(_scroll_indicator, PIP_SIZE_ACTIVE, 0);     // Add padding equal to pip size
    lv_obj_set_style_pad_bottom(_scroll_indicator, PIP_SIZE_ACTIVE, 0);  // Add padding equal to pip size
    lv_obj_set_flex_flow(_scroll_indicator, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(_scroll_indicator, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(_scroll_indicator, LV_OBJ_FLAG_SCROLLABLE);     // Disable scrolling

    // Create indicator pips
    for(int i = 0; i < _num_cards; i++) {
        lv_obj_t* pip = lv_obj_create(_scroll_indicator);
        lv_obj_set_size(pip, PIP_SIZE, PIP_SIZE);
        lv_obj_set_style_radius(pip, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_color(pip, lv_color_hex(0x808080), 0);
        lv_obj_set_style_border_width(pip, 0, 0);
    }

    // Set first pip as active
    lv_obj_t* first_pip = lv_obj_get_child(_scroll_indicator, 0);
    lv_obj_set_size(first_pip, PIP_SIZE_ACTIVE, PIP_SIZE_ACTIVE);
    lv_obj_set_style_bg_color(first_pip, lv_color_white(), 0);
}

void CardNavigationStack::addCard(lv_color_t color, const char* label_text) {
    lv_obj_t* card = lv_obj_create(_main_container);
    lv_obj_set_size(card, lv_pct(100), _height);  // Full width and height
    lv_obj_set_style_radius(card, 8, 0);
    lv_obj_set_style_border_width(card, 0, 0);
    lv_obj_set_style_bg_color(card, color, 0);
    lv_obj_set_style_pad_all(card, 0, 0);
    
    lv_obj_t* label = lv_label_create(card);
    lv_label_set_text(label, label_text);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_18, 0);
    lv_obj_center(label);
    lv_obj_set_style_text_color(label, lv_color_white(), 0);
    
    // Make sure the first card is centered on initial load
    if (lv_obj_get_child_cnt(_main_container) == 1) {
        lv_obj_scroll_to_view(card, LV_ANIM_OFF);
    }
}

void CardNavigationStack::nextCard() {
    _current_card = (_current_card < _num_cards - 1) ? _current_card + 1 : 0;
    goToCard(_current_card);
}

void CardNavigationStack::prevCard() {
    _current_card = (_current_card > 0) ? _current_card - 1 : _num_cards - 1;
    goToCard(_current_card);
}

void CardNavigationStack::goToCard(uint8_t index) {
    if (index >= _num_cards) return;
    
    _current_card = index;
    lv_obj_t* target_card = lv_obj_get_child(_main_container, _current_card);
    lv_obj_scroll_to_view(target_card, LV_ANIM_ON);
    _update_scroll_indicator(_current_card);
}

uint8_t CardNavigationStack::getCurrentIndex() const {
    return _current_card;
}

void CardNavigationStack::setMutex(SemaphoreHandle_t* mutex_ptr) {
    _mutex_ptr = mutex_ptr;
}

void CardNavigationStack::handleButtonPress(uint8_t button_index) {
    // Next card (Button 0)
    if (button_index == 0) {
        nextCard();
    }
    // Previous card (Button 2)
    else if (button_index == 2) {
        prevCard();
    }
}

void CardNavigationStack::buttonTask(void* parameter) {
    if (!_instance) return;
    
    while (1) {
        // Update all buttons
        for (int i = 0; i < NUM_BUTTONS; i++) {
            buttons[i].update();
            
            if (buttons[i].pressed()) {
                bool mutex_taken = false;
                
                // Take mutex if available
                if (_instance->_mutex_ptr != nullptr) {
                    mutex_taken = (xSemaphoreTake(*(_instance->_mutex_ptr), portMAX_DELAY) == pdTRUE);
                }
                
                // Handle button press
                _instance->handleButtonPress(i);
                
                // Release mutex if taken
                if (mutex_taken) {
                    xSemaphoreGive(*(_instance->_mutex_ptr));
                }
            }
        }

        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

void CardNavigationStack::_scroll_event_cb(lv_event_t* e) {
    lv_obj_t* cont = lv_event_get_target(e);
    lv_area_t cont_a;
    lv_obj_get_coords(cont, &cont_a);
    int32_t cont_y_center = cont_a.y1 + lv_area_get_height(&cont_a) / 2;

    uint32_t child_cnt = lv_obj_get_child_cnt(cont);
    
    for(uint32_t i = 0; i < child_cnt; i++) {
        lv_obj_t* child = lv_obj_get_child(cont, i);
        // Remove all opacity effects - everything is fully opaque
        lv_obj_set_style_opa(child, LV_OPA_COVER, 0);
    }
}

void CardNavigationStack::_update_scroll_indicator(int active_index) {
    static int previous_index = 0;  // Keep track of previous active pip
    
    if (previous_index != active_index) {
        // Animate previous pip to shrink
        lv_obj_t* prev_pip = lv_obj_get_child(_scroll_indicator, previous_index);
        lv_anim_t a;
        lv_anim_init(&a);
        lv_anim_set_var(&a, prev_pip);
        lv_anim_set_time(&a, PIP_ANIM_DURATION);
        lv_anim_set_values(&a, PIP_SIZE_ACTIVE, PIP_SIZE);
        lv_anim_set_exec_cb(&a, (lv_anim_exec_xcb_t)lv_obj_set_width);
        lv_obj_set_style_bg_color(prev_pip, lv_color_hex(0x808080), 0);
        lv_anim_start(&a);

        lv_anim_t a2;
        lv_anim_init(&a2);
        lv_anim_set_var(&a2, prev_pip);
        lv_anim_set_time(&a2, PIP_ANIM_DURATION);
        lv_anim_set_values(&a2, PIP_SIZE_ACTIVE, PIP_SIZE);
        lv_anim_set_exec_cb(&a2, (lv_anim_exec_xcb_t)lv_obj_set_height);
        lv_anim_start(&a2);

        // Animate new pip to grow
        lv_obj_t* new_pip = lv_obj_get_child(_scroll_indicator, active_index);
        lv_anim_t b;
        lv_anim_init(&b);
        lv_anim_set_var(&b, new_pip);
        lv_anim_set_time(&b, PIP_ANIM_DURATION);
        lv_anim_set_values(&b, PIP_SIZE, PIP_SIZE_ACTIVE);
        lv_anim_set_exec_cb(&b, (lv_anim_exec_xcb_t)lv_obj_set_width);
        lv_obj_set_style_bg_color(new_pip, lv_color_white(), 0);
        lv_anim_start(&b);

        lv_anim_t b2;
        lv_anim_init(&b2);
        lv_anim_set_var(&b2, new_pip);
        lv_anim_set_time(&b2, PIP_ANIM_DURATION);
        lv_anim_set_values(&b2, PIP_SIZE, PIP_SIZE_ACTIVE);
        lv_anim_set_exec_cb(&b2, (lv_anim_exec_xcb_t)lv_obj_set_height);
        lv_anim_start(&b2);

        previous_index = active_index;
    }
}