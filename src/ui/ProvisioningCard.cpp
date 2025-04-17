#include "ProvisioningCard.h"
#include "Style.h"

ProvisioningCard::ProvisioningCard(lv_obj_t* parent, WiFiInterface& wifiInterface, uint16_t width, uint16_t height)
    : _parent(parent), _wifiInterface(wifiInterface), _width(width), _height(height),
    _card(nullptr), _qrScreen(nullptr), _statusScreen(nullptr) {
    
    // Create main card container - use full width of parent
    _card = lv_obj_create(_parent);
    lv_obj_set_size(_card, LV_PCT(100), height);
    lv_obj_set_style_bg_color(_card, Style::backgroundColor(), 0);
    lv_obj_set_style_pad_all(_card, 0, 0);
    lv_obj_set_style_radius(_card, 8, 0);
    lv_obj_set_style_border_width(_card, 0, 0);
    
    // Create both screens as children of the card
    _qrScreen = lv_obj_create(_card);
    _statusScreen = lv_obj_create(_card);
    
    // Configure screens to take full card size
    lv_obj_set_size(_qrScreen, LV_PCT(100), LV_PCT(100));
    lv_obj_set_size(_statusScreen, LV_PCT(100), LV_PCT(100));
    
    // Set screen backgrounds to black
    lv_obj_set_style_bg_color(_qrScreen, Style::backgroundColor(), 0);
    lv_obj_set_style_bg_color(_statusScreen, Style::backgroundColor(), 0);
    
    // Position screens at 0,0 relative to card
    lv_obj_set_pos(_qrScreen, 0, 0);
    lv_obj_set_pos(_statusScreen, 0, 0);
    
    // Create the screen contents
    createQRScreen();
    createStatusScreen();
    
    // Initially show QR screen and hide status screen
    lv_obj_clear_flag(_qrScreen, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(_statusScreen, LV_OBJ_FLAG_HIDDEN);
}

void ProvisioningCard::updateConnectionStatus(const String& status) {
    // Create a static dispatch method to safely update UI from any thread
    static auto dispatchToUI = [](lv_obj_t* label, const String& text) {
        // Make a copy of the string for the callback
        auto* callback = new std::pair<lv_obj_t*, String>(label, text);
        
        lv_timer_t* timer = lv_timer_create([](lv_timer_t* timer) {
            auto* params = static_cast<std::pair<lv_obj_t*, String>*>(lv_timer_get_user_data(timer));
            if (params && params->first && lv_obj_is_valid(params->first)) {
                lv_label_set_text(params->first, params->second.c_str());
            }
            delete params;
            lv_timer_del(timer);
        }, 0, callback);
    };
    
    // Dispatch the UI update instead of doing it directly
    dispatchToUI(_statusLabel, status);
    showWiFiStatus();
}

void ProvisioningCard::updateIPAddress(const String& ip) {
    // Create a static dispatch method to safely update UI from any thread
    static auto dispatchToUI = [](lv_obj_t* label, const String& text) {
        // Make a copy of the string for the callback
        auto* callback = new std::pair<lv_obj_t*, String>(label, text);
        
        lv_timer_t* timer = lv_timer_create([](lv_timer_t* timer) {
            auto* params = static_cast<std::pair<lv_obj_t*, String>*>(lv_timer_get_user_data(timer));
            if (params && params->first && lv_obj_is_valid(params->first)) {
                lv_label_set_text(params->first, params->second.c_str());
            }
            delete params;
            lv_timer_del(timer);
        }, 0, callback);
    };
    
    // Dispatch the UI update instead of doing it directly
    dispatchToUI(_ipLabel, ip);
}

void ProvisioningCard::updateSignalStrength(int strength) {
    // Create a static dispatch method to safely update UI from any thread
    static auto dispatchToUI = [](lv_obj_t* label, int value) {
        auto* callback = new std::pair<lv_obj_t*, int>(label, value);
        
        lv_timer_t* timer = lv_timer_create([](lv_timer_t* timer) {
            auto* params = static_cast<std::pair<lv_obj_t*, int>*>(lv_timer_get_user_data(timer));
            if (params && params->first && lv_obj_is_valid(params->first)) {
                lv_label_set_text(params->first, (String(params->second) + "%").c_str());
            }
            delete params;
            lv_timer_del(timer);
        }, 0, callback);
    };
    
    // Dispatch the UI update instead of doing it directly
    dispatchToUI(_signalLabel, strength);
}

void ProvisioningCard::showQRCode() {
    // Generate QR code for the AP
    String ssid = _wifiInterface.getSSID();
    
    // Create QR code for WiFi network
    String qrData = generateQRCodeData(ssid, "");
    
    // Dispatch visibility changes and QR update to UI thread
    static auto dispatchToUI = [](lv_obj_t* show_screen, lv_obj_t* hide_screen, lv_obj_t* qr_code, const String& qr_data) {
        // Create struct for callback data
        struct QRUpdateData {
            lv_obj_t* show_screen;
            lv_obj_t* hide_screen;
            lv_obj_t* qr_code;
            String qr_data;
            
            QRUpdateData(lv_obj_t* s, lv_obj_t* h, lv_obj_t* q, const String& d) 
                : show_screen(s), hide_screen(h), qr_code(q), qr_data(d) {}
        };
        
        auto* data = new QRUpdateData(show_screen, hide_screen, qr_code, qr_data);
        
        lv_timer_t* timer = lv_timer_create([](lv_timer_t* timer) {
            auto* params = static_cast<QRUpdateData*>(lv_timer_get_user_data(timer));
            if (params) {
                // Update screen visibility
                if (params->show_screen && lv_obj_is_valid(params->show_screen)) {
                    lv_obj_clear_flag(params->show_screen, LV_OBJ_FLAG_HIDDEN);
                }
                if (params->hide_screen && lv_obj_is_valid(params->hide_screen)) {
                    lv_obj_add_flag(params->hide_screen, LV_OBJ_FLAG_HIDDEN);
                }
                
                // Update QR code
                if (params->qr_code && lv_obj_is_valid(params->qr_code)) {
                    lv_qrcode_update(params->qr_code, params->qr_data.c_str(), params->qr_data.length());
                }
            }
            delete params;
            lv_timer_del(timer);
        }, 0, data);
    };
    
    // Show QR screen, hide status screen, and update QR code
    dispatchToUI(_qrScreen, _statusScreen, _qrCode, qrData);
}

void ProvisioningCard::showWiFiStatus() {
    // Dispatch screen visibility changes to the UI thread
    static auto dispatchToUI = [](lv_obj_t* show_screen, lv_obj_t* hide_screen) {
        auto* screens = new std::pair<lv_obj_t*, lv_obj_t*>(show_screen, hide_screen);
        
        lv_timer_t* timer = lv_timer_create([](lv_timer_t* timer) {
            auto* params = static_cast<std::pair<lv_obj_t*, lv_obj_t*>*>(lv_timer_get_user_data(timer));
            if (params) {
                if (params->first && lv_obj_is_valid(params->first)) {
                    lv_obj_clear_flag(params->first, LV_OBJ_FLAG_HIDDEN);
                }
                if (params->second && lv_obj_is_valid(params->second)) {
                    lv_obj_add_flag(params->second, LV_OBJ_FLAG_HIDDEN);
                }
            }
            delete params;
            lv_timer_del(timer);
        }, 0, screens);
    };
    
    // Show status screen and hide QR screen
    dispatchToUI(_statusScreen, _qrScreen);
}

void ProvisioningCard::createQRScreen() {
    lv_obj_set_style_bg_color(_qrScreen, Style::backgroundColor(), 0);
    lv_obj_set_style_pad_all(_qrScreen, 0, 0);
    lv_obj_set_style_border_width(_qrScreen, 0, 0);
    
    // Create QR code with LVGL's built-in component
    // Use a slightly smaller size to ensure it fits within the screen
    const int qr_size = _height - 20;  // Leave some padding
    
    // Create a placeholder QR code with empty data (will be updated later)
    _qrCode = lv_qrcode_create(_qrScreen);
    // In LVGL v9, we set properties separately
    lv_qrcode_set_size(_qrCode, qr_size);
    lv_qrcode_set_dark_color(_qrCode, lv_color_black());
    lv_qrcode_set_light_color(_qrCode, lv_color_white()); 
    lv_obj_center(_qrCode);
    lv_qrcode_update(_qrCode, "WIFI:T:WPA;", 10); // Placeholder text
}

void ProvisioningCard::createStatusScreen() {
    lv_obj_set_style_bg_color(_statusScreen, Style::backgroundColor(), 0);
    lv_obj_set_style_pad_all(_statusScreen, 0, 0);
    lv_obj_set_style_border_width(_statusScreen, 0, 0);
    
    // Define semantic colors
    lv_color_t rowLabel = Style::labelColor();
    
    // Create container for status items - use full width of parent
    lv_obj_t* table = lv_obj_create(_statusScreen);
    lv_obj_set_size(table, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_style_pad_left(table, 5, 0);  // 5px from left edge
    lv_obj_set_style_pad_right(table, 5, 0);  // 5px from right edge
    lv_obj_set_style_pad_top(table, 0, 0);
    lv_obj_set_style_pad_bottom(table, 0, 0);
    lv_obj_set_style_border_width(table, 0, 0);
    lv_obj_set_style_bg_opa(table, 0, 0);
    
    // Create rows with title and right-aligned value labels
    createTableRow(table, 0, "WiFi", &_statusLabel, rowLabel);
    createTableRow(table, 1, "IP", &_ipLabel, rowLabel);
    createTableRow(table, 2, "Signal", &_signalLabel, rowLabel);
    
    // Set initial values
    lv_label_set_text(_statusLabel, "Disconnected");
    lv_label_set_text(_ipLabel, "");
    lv_label_set_text(_signalLabel, "0%");
}

void ProvisioningCard::createTableRow(lv_obj_t* table, uint16_t row, const char* title, lv_obj_t** valueLabel, lv_color_t labelColor) {
    // Calculate line height based on font
    int lineHeight = lv_font_get_line_height(Style::valueFont()) + 5;
    
    // Create container for the row content
    lv_obj_t* container = lv_obj_create(table);
    lv_obj_set_size(container, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_style_pad_all(container, 0, 0);
    lv_obj_set_style_bg_opa(container, 0, 0);
    lv_obj_set_style_border_width(container, 0, 0);
    lv_obj_set_pos(container, 0, row * lineHeight);
    
    // Create title label (left-aligned)
    lv_obj_t* titleLabel = lv_label_create(container);
    lv_obj_set_style_text_font(titleLabel, Style::labelFont(), 0);
    lv_obj_set_style_text_color(titleLabel, labelColor, 0);
    lv_label_set_text(titleLabel, title);
    lv_obj_align(titleLabel, LV_ALIGN_LEFT_MID, 0, 0);
    
    // Create value label (right-aligned)
    *valueLabel = lv_label_create(container);
    lv_obj_set_style_text_font(*valueLabel, Style::valueFont(), 0);
    lv_obj_set_style_text_color(*valueLabel, Style::valueColor(), 0);
    lv_obj_align(*valueLabel, LV_ALIGN_RIGHT_MID, 0, 0);
}

String ProvisioningCard::generateQRCodeData(const String& ssid, const String& password) {
    // For open networks
    if (password.length() == 0) {
        return "WIFI:S:" + ssid + ";T:nopass;;";
    }
    
    // For password-protected networks
    return "WIFI:S:" + ssid + ";T:WPA;P:" + password + ";;";
}

