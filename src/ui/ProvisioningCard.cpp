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
    lv_label_set_text(_statusLabel, status.c_str());
    showWiFiStatus();
}

void ProvisioningCard::updateIPAddress(const String& ip) {
    lv_label_set_text(_ipLabel, ip.c_str());
}

void ProvisioningCard::updateSignalStrength(int strength) {
    lv_label_set_text(_signalLabel, (String(strength) + "%").c_str());
}

void ProvisioningCard::showQRCode() {
    // Show QR screen and hide status screen
    lv_obj_clear_flag(_qrScreen, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(_statusScreen, LV_OBJ_FLAG_HIDDEN);
    
    // Generate QR code for the AP
    String ssid = _wifiInterface.getSSID();
    
    // Create QR code for WiFi network
    String qrData = generateQRCodeData(ssid, "");
    
    // Update QR code with new data
    lv_qrcode_update(_qrCode, qrData.c_str(), qrData.length());
}

void ProvisioningCard::showWiFiStatus() {
    // Show status screen and hide QR screen
    lv_obj_clear_flag(_statusScreen, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(_qrScreen, LV_OBJ_FLAG_HIDDEN);
}

void ProvisioningCard::createQRScreen() {
    // QR codes need to have high contrast - white background with black pattern
    lv_obj_set_style_bg_color(_qrScreen, lv_color_white(), 0);
    lv_obj_set_style_pad_all(_qrScreen, 0, 0);
    lv_obj_set_style_border_width(_qrScreen, 0, 0);
    
    // Create QR code with LVGL's built-in component
    // Use a slightly smaller size to ensure it fits within the screen
    const int qr_size = _height - 20;  // Leave some padding
    
    // Create a placeholder QR code with empty data (will be updated later)
    _qrCode = lv_qrcode_create(_qrScreen, qr_size, lv_color_black(), lv_color_white());
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

