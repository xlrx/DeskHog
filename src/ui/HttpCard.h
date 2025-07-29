#pragma once

#include <lvgl.h>
#include <Arduino.h>
#include <HTTPClient.h>
#include "ui/InputHandler.h"

class HttpCard : public InputHandler {
public:
    HttpCard(lv_obj_t* parent, const String& url, bool asNumber, uint32_t refreshIntervalSec);
    ~HttpCard();

    lv_obj_t* getCard() const { return _card; }

    bool update() override;
    bool handleButtonPress(uint8_t button_index) override;
    void prepareForRemoval() override { _card = nullptr; }

private:
    void fetchData();

    lv_obj_t* _card;
    lv_obj_t* _label;

    String _url;
    bool _asNumber;
    uint32_t _refreshIntervalMs;
    unsigned long _lastFetch;
};
