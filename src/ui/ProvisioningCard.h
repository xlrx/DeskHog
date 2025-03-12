#ifndef PROVISION_UI_H
#define PROVISION_UI_H

#include <Arduino.h>
#include <lvgl.h>
#include "hardware/WifiInterface.h"

class ProvisioningCard {
public:
    // Constructor
    ProvisioningCard(lv_obj_t* parent, WiFiInterface& wifiInterface, uint16_t width, uint16_t height);

    // Update connection status
    void updateConnectionStatus(const String& status);

    // Update IP address
    void updateIPAddress(const String& ip);

    // Update signal strength (0-100%)
    void updateSignalStrength(int strength);

    // Show QR code for AP connection
    void showQRCode();

    // Show WiFi status screen
    void showWiFiStatus();

private:
    // Screen dimensions
    uint16_t _width;
    uint16_t _height;

    // Parent container
    lv_obj_t* _parent;

    // WiFi Manager reference
    WiFiInterface& _wifiInterface;

    // UI screens
    lv_obj_t* _qrScreen;     // QR code screen
    lv_obj_t* _statusScreen; // WiFi status screen

    // UI elements for status screen
    lv_obj_t* _statusLabel;
    lv_obj_t* _ipLabel;
    lv_obj_t* _signalIcon;
    lv_obj_t* _signalLabel;
    lv_obj_t* _ssidLabel;

    // QR code objects
    lv_obj_t* _qrCode;

    // Create QR code screen
    void createQRScreen();

    // Create WiFi status screen
    void createStatusScreen();

    // Generate QR code data
    String generateQRCodeData(const String& ssid, const String& password);

    // Signal strength icon path based on strength (0-100%)
    const char* getSignalIconPath(int strength);
};

#endif // PROVISION_UI_H