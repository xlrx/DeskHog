#include "ProvisioningCard.h"

ProvisioningCard::ProvisioningCard(lv_obj_t* parent, WiFiInterface& wifiInterface, uint16_t width, uint16_t height)
    : _parent(parent), _wifiInterface(wifiInterface), _width(width), _height(height),
    _card(nullptr), _qrScreen(nullptr), _statusScreen(nullptr) {
    
    // Create main card container
    _card = lv_obj_create(_parent);
    lv_obj_set_size(_card, width, height);
    lv_obj_set_style_bg_color(_card, lv_color_white(), 0);
    lv_obj_set_style_pad_all(_card, 0, 0);
    lv_obj_set_style_radius(_card, 8, 0);
    lv_obj_set_style_border_width(_card, 0, 0);
    
    // Create both screens as children of the card
    _qrScreen = lv_obj_create(_card);
    _statusScreen = lv_obj_create(_card);
    
    // Configure screens to take full card size
    lv_obj_set_size(_qrScreen, width, height);
    lv_obj_set_size(_statusScreen, width, height);
    
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
    String ipText = "IP: " + ip;
    lv_label_set_text(_ipLabel, ipText.c_str());
}

void ProvisioningCard::updateSignalStrength(int strength) {
    lv_label_set_text(_signalLabel, (String(strength) + "%").c_str());
    
    // Update the signal icon based on strength
    const char* iconPath = getSignalIconPath(strength);
    // In a real implementation, you would set an image or icon here
    // This is just a placeholder for demonstration
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
    
    // Update SSID label
    String ssidText = "Network: " + _wifiInterface.getSSID();
    lv_label_set_text(_ssidLabel, ssidText.c_str());
}

void ProvisioningCard::createQRScreen() {
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
    lv_obj_set_style_bg_color(_statusScreen, lv_color_white(), 0);
    lv_obj_set_style_pad_all(_statusScreen, 10, 0);
    lv_obj_set_style_border_width(_statusScreen, 0, 0);
    
    // Create title
    lv_obj_t* title = lv_label_create(_statusScreen);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 5);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_16, 0);
    lv_label_set_text(title, "WiFi Status");
    
    // Create SSID label
    _ssidLabel = lv_label_create(_statusScreen);
    lv_obj_align(_ssidLabel, LV_ALIGN_TOP_LEFT, 0, 30);
    lv_obj_set_style_text_font(_ssidLabel, &lv_font_montserrat_18, 0);
    lv_label_set_text(_ssidLabel, "Network: ");
    
    // Create status label
    _statusLabel = lv_label_create(_statusScreen);
    lv_obj_align(_statusLabel, LV_ALIGN_TOP_LEFT, 0, 50);
    lv_obj_set_style_text_font(_statusLabel, &lv_font_montserrat_18, 0);
    lv_label_set_text(_statusLabel, "Status: Disconnected");
    
    // Create IP address label
    _ipLabel = lv_label_create(_statusScreen);
    lv_obj_align(_ipLabel, LV_ALIGN_TOP_LEFT, 0, 70);
    lv_obj_set_style_text_font(_ipLabel, &lv_font_montserrat_18, 0);
    lv_label_set_text(_ipLabel, "IP: ");
    
    // Create signal strength section
    lv_obj_t* signalContainer = lv_obj_create(_statusScreen);
    lv_obj_remove_style_all(signalContainer);
    lv_obj_set_size(signalContainer, 100, 30);
    lv_obj_align(signalContainer, LV_ALIGN_TOP_LEFT, 0, 90);
    
    // Signal strength label
    lv_obj_t* signalText = lv_label_create(signalContainer);
    lv_obj_align(signalText, LV_ALIGN_LEFT_MID, 0, 0);
    lv_label_set_text(signalText, "Signal: ");
    
    // Signal strength percentage
    _signalLabel = lv_label_create(signalContainer);
    lv_obj_align(_signalLabel, LV_ALIGN_LEFT_MID, 60, 0);
    lv_label_set_text(_signalLabel, "0%");
    
    // Signal strength icon
    _signalIcon = lv_obj_create(signalContainer);
    lv_obj_set_size(_signalIcon, 20, 20);
    lv_obj_align(_signalIcon, LV_ALIGN_RIGHT_MID, 0, 0);
}

String ProvisioningCard::generateQRCodeData(const String& ssid, const String& password) {
    // For open networks
    if (password.length() == 0) {
        return "WIFI:S:" + ssid + ";T:nopass;;";
    }
    
    // For password-protected networks
    return "WIFI:S:" + ssid + ";T:WPA;P:" + password + ";;";
}

const char* ProvisioningCard::getSignalIconPath(int strength) {
    // In a real implementation, you would return different icon paths based on strength
    // This is just a placeholder
    if (strength > 70) {
        return "signal_strong";
    } else if (strength > 40) {
        return "signal_medium";
    } else if (strength > 10) {
        return "signal_weak";
    } else {
        return "signal_none";
    }
}