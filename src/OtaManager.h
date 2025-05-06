#pragma once

#include <Arduino.h>
#include <ArduinoJson.h> // For potential JSON parsing within the manager

// FreeRTOS for task management
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// Forward declarations if needed, e.g., if using WiFiClientSecure pointer
// class WiFiClientSecure;

// Structure to hold information about an available update
struct UpdateInfo {
    bool updateAvailable = false;
    String currentVersion = "";
    String availableVersion = "";
    String downloadUrl = "";
    String releaseNotes = ""; // Optional
    String error = ""; // Store error messages
};

// Structure to hold the status of an ongoing update
struct UpdateStatus {
    enum class State {
        IDLE,
        CHECKING_VERSION,
        DOWNLOADING,
        WRITING,
        SUCCESS,
        ERROR_WIFI,
        ERROR_HTTP_CHECK,
        ERROR_HTTP_DOWNLOAD,
        ERROR_JSON,
        ERROR_UPDATE_BEGIN,
        ERROR_UPDATE_WRITE,
        ERROR_UPDATE_END,
        ERROR_NO_ASSET,
        ERROR_NO_SPACE
    };

    State status = State::IDLE;
    int progress = 0; // Percentage (0-100)
    String message = "Idle";
};

class OtaManager {
public:
    /**
     * @brief Constructor
     * @param currentVersion The firmware version currently running.
     * @param repoOwner GitHub repository owner (e.g., "PostHog").
     * @param repoName GitHub repository name (e.g., "DeskHog").
     */
    OtaManager(const String& currentVersion, const String& repoOwner, const String& repoName);

    /**
     * @brief Initiates a check for firmware updates in a non-blocking manner.
     * Spawns a FreeRTOS task to perform the actual check.
     * @return true if the check task was successfully started, false otherwise (e.g., already checking, WiFi not connected).
     */
    bool checkForUpdate();

    /**
     * @brief Begin the firmware update process using the provided download URL.
     * This function initiates the download and flashing process.
     * It should ideally run asynchronously (e.g., in a task) and update status internally.
     * @param downloadUrl The direct HTTPS URL to the firmware .bin file.
     * @return true if the update process was successfully initiated, false otherwise (e.g., update already in progress, invalid URL).
     */
    bool beginUpdate(const String& downloadUrl);

    /**
     * @brief Get the current status of the OTA process.
     * Non-blocking, returns the latest status tracked internally.
     * @return UpdateStatus struct.
     */
    UpdateStatus getStatus();

    UpdateInfo getLastCheckResult(); // Added to retrieve the result of the last check

    /**
      * @brief Process loop for the OtaManager.
      * Should be called periodically to handle asynchronous operations like download/write.
      * If using tasks, this might not be strictly necessary, depends on implementation.
      */
    void process(); // Optional, depending on async approach

private:
    String _currentVersion;
    String _repoOwner;
    String _repoName;
    UpdateStatus _currentStatus;
    UpdateInfo _lastCheckResult; // Cache the result of the last check
    TaskHandle_t _checkTaskHandle = NULL; // Handle for the update check task
    TaskHandle_t _updateTaskHandle = NULL; // Handle for the firmware update task

    // Placeholder for the name of the asset to download (e.g., "firmware.bin")
    // Needs to be set based on project convention
    const char* _firmwareAssetName = "firmware.bin";

    // DigiCert Global Root G2 certificate for api.github.com
    // Valid until: Jan 15 12:00:00 2038 GMT
    // TODO: Confirm this is still the correct root CA for api.github.com if issues arise
    const char* _githubApiRootCa = \
        "-----BEGIN CERTIFICATE-----\n" \
        "MIIDjjCCAnagAwIBAgIQAzrx5qcRqaC7KGSxHQn65TANBgkqhkiG9w0BAQsFADBh\n" \
        "MQswCQYDVQQGEwJVUzEVMBMGA1UEChMMRGlnaUNlcnQgSW5jMRkwFwYDVQQLExB3\n" \
        "d3cuZGlnaWNlcnQuY29tMSAwHgYDVQQDExdEaWdpQ2VydCBHbG9iYWwgUm9vdCBH\n" \
        "MjAeFw0xMzA4MDExMjAwMDBaFw0zODAxMTUxMjAwMDBaMGExCzAJBgNVBAYTAlVT\n" \
        "MRUwEwYDVQQKEwxEaWdpQ2VydCBJbmMxGTAXBgNVBAsTEHd3dy5kaWdpY2VydC5j\n" \
        "b20xIDAeBgNVBAMTF0RpZ2lDZXJ0IEdsb2JhbCBSb290IEcyMIIBIjANBgkqhkiG\n" \
        "9w0BAQEFAAOCAQ8AMIIBCgKCAQEAuzfNNNx7a8myaJCtSnX/RrohCgiN9RlUyfuI\n" \
        "2/Ou8jqJkTx65qsGGmvPrC3oXgkkRLpimn7Wo6h+4FR1IAWsULecYxpsMNzaHxmx\n" \
        "1x7e/dfgy5SDN67sH0NO3Xss0r0upS/kqbitOtSZpLYl6ZtrAGCSYP9PIUkY92eQ\n" \
        "q2EGnI/yuum06ZIya7XzV+hdG82MHauVBJVJ8zUtluNJbd134/tJS7SsVQepj5Wz\n" \
        "tCO7TG1F8PapspUwtP1MVYwnSlcUfIKdzXOS0xZKBgyMUNGPHgm+F6HmIcr9g+UQ\n" \
        "vIOlCsRnKPZzFBQ9RnbDhxSJITRNrw9FDKZJobq7nMWxM4MphQIDAQABo0IwQDAP\n" \
        "BgNVHRMBAf8EBTADAQH/MA4GA1UdDwEB/wQEAwIBhjAdBgNVHQ4EFgQUTiJUIBiV\n" \
        "5uNu5g/6+rkS7QYXjzkwDQYJKoZIhvcNAQELBQADggEBAGBnKJRvDkhj6zHd6mcY\n" \
        "1Yl9PMWLSn/pvtsrF9+wX3N3KjITOYFnQoQj8kVnNeyIv/iPsGEMNKSuIEyExtv4\n" \
        "NeF22d+mQrvHRAiGfzZ0JFrabA0UWTW98kndth/Jsw1HRj2ZL7tcu7XUIOGZX1NG\n" \
        "Fdtom/DzMNU+MeKNhJ7jitralj41E6Vf8PlwUHBHQRFXGU7Aj64GxJUTFy8bJZ91\n" \
        "8rGOmaFvE7FBcf6IKshPECBV1/MUReXgRPTqh5Uykw7+U0b6LJ3/iyK5S9kJRaTe\n" \
        "pLiaWN0bfVKfjllDiIGknibVb63dDcY3fe0Dkhvld1927jyNxF1WW6LZZm6zNTfl\n" \
        "MrY=\n" \
        "-----END CERTIFICATE-----\n";

    // Task function
    static void _checkUpdateTaskRunner(void* pvParameters);
    static void _updateTaskRunner(void* pvParameters);

    // Private helper method prototypes
    void _setUpdateStatus(UpdateStatus::State state, const String& message, int progress = -1);
    String _performHttpsRequest(const char* url, const char* rootCa);
    UpdateInfo _parseGithubApiResponse(const String& jsonPayload);

    // void _performUpdate(String url); // Function to run in a task
}; 