#include "ui/HttpCard.h"
#include "Style.h"
#include <WiFi.h>

HttpCard::HttpCard(lv_obj_t* parent, const String& url, bool asNumber, uint32_t refreshIntervalSec)
    : _card(nullptr), _label(nullptr), _url(url), _asNumber(asNumber),
      _refreshIntervalMs(refreshIntervalSec * 1000), _lastFetch(0) {
    _card = lv_obj_create(parent);
    lv_obj_set_size(_card, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(_card, lv_color_black(), 0);

    _label = lv_label_create(_card);
    lv_obj_set_style_text_color(_label, lv_color_white(), 0);
    lv_obj_set_style_text_font(_label, Style::largeValueFont(), 0);
    lv_label_set_text(_label, "Loading...");
    lv_obj_center(_label);
}

HttpCard::~HttpCard() {
    if (_card) {
        lv_obj_del_async(_card);
        _card = nullptr;
    }
}

bool HttpCard::update() {
    if (!_card) return false;
    unsigned long now = millis();
    if (now - _lastFetch >= _refreshIntervalMs) {
        fetchData();
        _lastFetch = now;
    }
    return true;
}

bool HttpCard::handleButtonPress(uint8_t button_index) {
    if (button_index == 1) { // center
        fetchData();
        return true;
    }
    return false;
}

void HttpCard::fetchData() {
    if (WiFi.status() != WL_CONNECTED) return;

    HTTPClient http;
    http.begin(_url);
    int code = http.GET();
    if (code > 0) {
        String body = http.getString();
        if (_asNumber) {
            double num = body.toDouble();
            char buf[32];
            snprintf(buf, sizeof(buf), "%.2f", num);
            lv_label_set_text(_label, buf);
        } else {
            lv_label_set_text(_label, body.c_str());
        }
    } else {
        Serial.printf("HttpCard GET failed: %d\n", code);
    }
    http.end();
}
