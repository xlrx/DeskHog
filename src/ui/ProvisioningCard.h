#pragma once

#include <lvgl.h>  // LVGL core library
#include <string>  // For String class (or could be Arduino's String)
#include "hardware/WiFiInterface.h"  // Custom WiFi interface class

/**
 * @class ProvisioningCard
 * @brief UI component for WiFi provisioning with QR code and status display
 * 
 * This class creates and manages a card in LVGL that can display:
 * - QR code for WiFi network connection
 * - Connection status information including IP and signal strength
 */
class ProvisioningCard {
public:
    /**
     * @brief Construct a new WiFi provisioning card UI component
     * 
     * @param parent Parent LVGL container
     * @param wifiInterface Reference to WiFi interface
     * @param width Card width
     * @param height Card height
     */
    ProvisioningCard(lv_obj_t* parent, WiFiInterface& wifiInterface, 
                     uint16_t width, uint16_t height);
    
    /**
     * @brief Get the underlying LVGL card object
     * 
     * @return lv_obj_t* Pointer to the LVGL card container
     */
    lv_obj_t* getCard() const;
    
    /**
     * @brief Updates connection status text and shows status screen
     * 
     * @param status WiFi connection status text
     */
    void updateConnectionStatus(const String& status);
    
    /**
     * @brief Updates IP address display
     * 
     * @param ip IP address string
     */
    void updateIPAddress(const String& ip);
    
    /**
     * @brief Updates signal strength percentage
     * 
     * @param strength Signal strength (0-100)
     */
    void updateSignalStrength(int strength);
    
    /**
     * @brief Generates and displays QR code for WiFi network
     */
    void showQRCode();
    
    /**
     * @brief Shows WiFi status screen and hides QR code screen
     */
    void showWiFiStatus();
    
private:
    /**
     * @brief Creates main card container
     */
    void createCard();
    
    /**
     * @brief Creates the QR code screen
     */
    void createQRScreen();
    
    /**
     * @brief Creates the status display screen
     */
    void createStatusScreen();
    
    /**
     * @brief Creates a table row with label and value
     * 
     * @param table Parent table object
     * @param row Row index (0-based)
     * @param title Label text
     * @param valueLabel Pointer to store created value label
     * @param labelColor Color for the label
     */
    void createTableRow(lv_obj_t* table, uint16_t row, const char* title, 
                      lv_obj_t** valueLabel, lv_color_t labelColor);
    
    /**
     * @brief Generates QR code data string for WiFi network
     * 
     * @param ssid Network SSID (escaped if needed)
     * @param password Network password (escaped if needed)
     * @return Formatted QR code data string
     */
    String generateQRCodeData(const String& ssid, const String& password);
    
    /**
     * @brief Escapes special characters in strings for QR code
     * 
     * @param input Input string
     * @return Escaped string
     */
    String escapeString(const String& input);
    
    /**
     * @brief Safely updates a label's text from any thread
     * 
     * @param label Label to update
     * @param text New text content
     */
    void safeUpdateLabel(lv_obj_t* label, const String& text);
    
    /**
     * @brief Safely toggles visibility between two screens
     * 
     * @param showScreen Screen to show
     * @param hideScreen Screen to hide
     */
    void toggleScreens(lv_obj_t* showScreen, lv_obj_t* hideScreen);
    
    // Member variables
    lv_obj_t* _parent;
    WiFiInterface& _wifiInterface;
    uint16_t _width;
    uint16_t _height;
    
    lv_obj_t* _card;
    lv_obj_t* _qrScreen;
    lv_obj_t* _statusScreen;
    lv_obj_t* _qrCode;
    lv_obj_t* _statusLabel;
    lv_obj_t* _ipLabel;
    lv_obj_t* _signalLabel;
};