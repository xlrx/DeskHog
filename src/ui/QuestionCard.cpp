#include "ui/QuestionCard.h"
#include "Style.h"

QuestionCard::QuestionCard(lv_obj_t* parent)
    : _card(nullptr)
    , _background(nullptr)
    , _label(nullptr)
    , _label_shadow(nullptr)
    , _current_question_index(0)
    , _questions()
    , _anim()
    , _cont(nullptr)
    , _shadow_cont(nullptr) {
    
    // Create main card with black background
    _card = lv_obj_create(parent);
    if (!_card) return;
    
    // Set card size and style
    lv_obj_set_width(_card, lv_pct(100));
    lv_obj_set_height(_card, lv_pct(100));
    lv_obj_set_style_bg_color(_card, lv_color_black(), 0);
    lv_obj_set_style_border_width(_card, 0, 0);
    lv_obj_set_style_pad_all(_card, 5, 0);
    lv_obj_set_style_margin_all(_card, 0, 0);
    
    // Create background container with rounded corners
    _background = lv_obj_create(_card);
    if (!_background) return;
    
    // Make background container fill parent completely
    lv_obj_set_style_radius(_background, 8, LV_PART_MAIN);
    lv_obj_set_style_bg_color(_background, lv_color_hex(0x4A4A4A), 0); // Dark gray background
    lv_obj_set_style_border_width(_background, 0, 0);
    lv_obj_set_style_pad_all(_background, 5, 0);
    
    lv_obj_set_width(_background, lv_pct(100));
    lv_obj_set_height(_background, lv_pct(100));
    
    // Create shadow label (black, 1px offset)
    _label_shadow = createStyledLabel(_background, lv_color_black(), 0, 1);
    
    // Create main label (white, no shadow offset)
    _label = createStyledLabel(_background, lv_color_white(), -1, 0);
    
    // Add some default questions
    addQuestion("If you had to describe your last weekend using only 3 emojis, what would they be?");
    addQuestion("If your name were the acronym for your next job title (e.g., ANNIKA = Astronaut Navigating New Interstellar Knowledge Adventures), what would it stand for?");
    addQuestion("What\'s the last photo you took on your phone that you'd be willing to share and explain?");
    addQuestion("What song title best describes your current mood or week?");
    addQuestion("What\'s a random topic you could give a 10-minute presentation on with no prep?");
    addQuestion("What\'s a trend you secretly (or not so secretly) loved?");
    addQuestion("Pineapple on pizza: yes or no?");

    // Display the first question
    if (!_questions.empty()) {
        setText(_questions[0].c_str());
    }
}

lv_obj_t* QuestionCard::createStyledLabel(lv_obj_t* parent, lv_color_t color, int16_t x_offset, int16_t y_offset) {
    // Create a scrollable container
    lv_obj_t* cont = lv_obj_create(parent);
    if (x_offset == -1) {  // Main label has x_offset = -1
        _cont = cont;  // Store main container
    } else {
        _shadow_cont = cont;  // Store shadow container
    }
    
    lv_obj_set_size(cont, lv_pct(90), lv_pct(80));
    lv_obj_set_style_bg_opa(cont, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(cont, 0, 0);
    lv_obj_set_scroll_dir(cont, LV_DIR_VER);
    lv_obj_set_scroll_snap_y(cont, LV_SCROLL_SNAP_CENTER);
    lv_obj_set_scrollbar_mode(cont, LV_SCROLLBAR_MODE_OFF);
    lv_obj_align(cont, LV_ALIGN_CENTER, x_offset, y_offset);

    // Create the label inside the container
    lv_obj_t* label = lv_label_create(cont);
    if (!label) return nullptr;
    
    lv_obj_set_style_text_font(label, Style::loudNoisesFont(), 0);
    lv_obj_set_style_text_color(label, color, 0);
    lv_label_set_long_mode(label, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(label, lv_pct(100));
    
    // Create continuous scrolling animation
    lv_anim_t a;  // Create local animation
    lv_anim_init(&a);
    lv_anim_set_var(&a, cont);
    lv_anim_set_exec_cb(&a, (lv_anim_exec_xcb_t)lv_obj_scroll_to_y);
    lv_anim_set_values(&a, 0, lv_obj_get_height(label));
    lv_anim_set_time(&a, 3000);
    lv_anim_set_playback_time(&a, 3000);
    lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
    lv_anim_start(&a);

    // Store animation in class member
    _anim = a;
    
    return label;
}

QuestionCard::~QuestionCard() {
    if (isValidObject(_card)) {
        lv_obj_add_flag(_card, LV_OBJ_FLAG_HIDDEN);
        lv_obj_del_async(_card);
        
        _card = nullptr;
        _background = nullptr;
        _label = nullptr;
        _label_shadow = nullptr;
    }
}

lv_obj_t* QuestionCard::getCard() {
    return _card;
}

void QuestionCard::addQuestion(const char* question) {
    _questions.push_back(question);
}

void QuestionCard::setText(const char* text) {
    if (_label) lv_label_set_text(_label, text);
    if (_label_shadow) lv_label_set_text(_label_shadow, text);
}

void QuestionCard::cycleNextQuestion() {
    if (_questions.empty()) return;
    
    _current_question_index = (_current_question_index + 1) % _questions.size();
    setText(_questions[_current_question_index].c_str());
    
    // Restart animation
    lv_anim_t a = _anim;  // Copy stored animation
    lv_anim_start(&a);    // Start new instance
    _anim = a;            // Store updated animation
    
    // Cycle through different background colors
    static const uint32_t colors[] = {
        0x4A4A4A,  // Dark gray (original)
        0x2C3E50,  // Dark blue
        0x27AE60,  // Green
        0x8E44AD,  // Purple
        0xE67E22,  // Orange
        0x2980B9,  // Blue
        0xC0392B   // Red
    };
    static const size_t num_colors = sizeof(colors) / sizeof(colors[0]);
    
    uint32_t new_color = colors[_current_question_index % num_colors];
    lv_obj_set_style_bg_color(_background, lv_color_hex(new_color), 0);
}

bool QuestionCard::handleButtonPress(uint8_t button_index) {
    if (button_index == 1) { // Center button
        cycleNextQuestion();
        return true;
    }
    return false;
}

bool QuestionCard::isValidObject(lv_obj_t* obj) const {
    return obj != nullptr && lv_obj_is_valid(obj);
}