#include "OtaManager.h"
#include <WiFi.h>             // For checking WiFi status
#include <WiFiClientSecure.h> // Required for HTTPS
#include <Update.h>         // Required for OTA flashing
#include <HTTPClient.h>     // Required for making HTTP requests
#include <ArduinoJson.h>    // Required for parsing GitHub API response
#include "esp_heap_caps.h"   // For heap_caps_malloc_extmem_enable

// NTP Configuration
const char* NTP_SERVER = "pool.ntp.org";
const long  GMT_OFFSET_SEC = 0;      // Configure for your timezone
const int   DAYLIGHT_OFFSET_SEC = 0; // Configure if DST applies

// mbedTLS debug callback function
#if CORE_DEBUG_LEVEL >= ARDUHAL_LOG_LEVEL_VERBOSE
static void mbedtls_debug_print(void *ctx, int level,
                                const char *file, int line,
                                const char *str)
{
    ((void) level); // Unused parameter
    Serial.printf("mbedtls: %s:%04d: %s", file, line, str);
}
#endif

// Define the current version (replace with actual versioning mechanism later)
// This should come from a central place, maybe a build flag or version.h
/* #ifndef CURRENT_FIRMWARE_VERSION
#define CURRENT_FIRMWARE_VERSION "0.0.0-dev"
#endif */

#define OTA_TASK_STACK_SIZE 10240 // Stack size for OTA tasks (HTTPS + JSON can be heavy)
#define OTA_TASK_PRIORITY 2     // Priority for OTA tasks

// Constructor
OtaManager::OtaManager(const String& currentVersion, const String& repoOwner, const String& repoName)
    : _currentVersion(currentVersion),
      _repoOwner(repoOwner),
      _repoName(repoName),
      _checkTaskHandle(NULL),
      _updateTaskHandle(NULL) { // Initialize task handles
    heap_caps_malloc_extmem_enable(4096); // Prefer PSRAM for allocations > 4KB
    _currentStatus.status = UpdateStatus::State::IDLE;
    _currentStatus.message = "Idle";
    _currentStatus.progress = 0;
    _lastCheckResult.currentVersion = _currentVersion; // Initialize last check result
}

// Static task runner for checking updates
void OtaManager::_checkUpdateTaskRunner(void* pvParameters) {
    OtaManager* self = static_cast<OtaManager*>(pvParameters);
    UpdateInfo resultInfo;
    resultInfo.currentVersion = self->_currentVersion;

    // Ensure time is synchronized before proceeding
    if (!self->_ensureTimeSynced()) {
        Serial.println("OTA Task Error: Time synchronization failed. Aborting update check task.");
        // Status might have been CHECKING_VERSION, change to IDLE with error
        self->_setUpdateStatus(UpdateStatus::State::IDLE, "Time sync failed", 0);
        resultInfo.error = "Time sync failed";
        self->_lastCheckResult = resultInfo; // Store the result with error
        self->_checkTaskHandle = NULL;      // Clear the task handle
        vTaskDelete(NULL);                // Task deletes itself
        return;
    }

    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("OTA Task Error: WiFi not connected at task start.");
        self->_setUpdateStatus(UpdateStatus::State::ERROR_WIFI, "WiFi not connected", 0);
        resultInfo.error = "WiFi not connected";
    } else {
        // Construct GitHub API URL
        String apiUrl = "https://api.github.com/repos/";
        apiUrl += self->_repoOwner;
        apiUrl += "/";
        apiUrl += self->_repoName;
        apiUrl += "/releases/latest";

        String jsonPayload = self->_performHttpsRequest(apiUrl.c_str(), self->_githubApiRootCa);
        resultInfo = self->_parseGithubApiResponse(jsonPayload);
    }

    self->_lastCheckResult = resultInfo; // Store the result

    // Update final status based on task outcome
    if (!resultInfo.error.isEmpty()) {
        self->_setUpdateStatus(UpdateStatus::State::IDLE, "Check failed: " + resultInfo.error, 0);
    } else if (!resultInfo.updateAvailable) {
        self->_setUpdateStatus(UpdateStatus::State::IDLE, "No update available", 0);
    } else {
        // If update is available, status remains CHECKING_VERSION to indicate readiness
        // It will be cleared by beginUpdate or a subsequent check.
        self->_setUpdateStatus(UpdateStatus::State::CHECKING_VERSION, "Update available: " + resultInfo.availableVersion, 0);
    }

    self->_checkTaskHandle = NULL; // Clear the task handle
    vTaskDelete(NULL); // Task deletes itself
}

// Private helper to ensure time is synchronized via NTP
bool OtaManager::_ensureTimeSynced() {
    if (_timeSynced) {
        Serial.println("OTA Info: Time is already synchronized.");
        return true;
    }

    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("OTA Error: WiFi not connected. Cannot sync time.");
        return false;
    }

    Serial.println("OTA Info: Configuring time from NTP server...");
    configTime(GMT_OFFSET_SEC, DAYLIGHT_OFFSET_SEC, NTP_SERVER);
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo, 10000)) { // Wait up to 10 seconds for time sync
        Serial.println("OTA Error: Failed to obtain time from NTP server.");
        _timeSynced = false;
        return false;
    }
    Serial.println("OTA Info: Time synchronized successfully.");
    Serial.print("Current time: ");
    Serial.println(asctime(&timeinfo));
    _timeSynced = true;
    return true;
}

// Initiate non-blocking check for updates
bool OtaManager::checkForUpdate() {
    if (_checkTaskHandle != NULL) {
        Serial.println("OTA Info: Update check already in progress.");
        // Optionally, could update status to reflect it's already checking
        // _setUpdateStatus(UpdateStatus::State::CHECKING_VERSION, "Check already in progress...", _currentStatus.progress);
        return false; // Indicate task not launched because one is active
    }

    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("OTA Error: WiFi not connected. Cannot start update check task.");
        _setUpdateStatus(UpdateStatus::State::ERROR_WIFI, "WiFi not connected", 0);
        _lastCheckResult.error = "WiFi not connected";
        _lastCheckResult.updateAvailable = false;
        // Quickly transition to IDLE if WiFi is off, so UI doesn't get stuck on "Checking..."
        _setUpdateStatus(UpdateStatus::State::IDLE, "Check failed: WiFi off", 0);
        return false;
    }

    _setUpdateStatus(UpdateStatus::State::CHECKING_VERSION, "Checking for updates...", 0);
    // Clear previous error on a new check attempt that starts successfully
    _lastCheckResult = UpdateInfo();
    _lastCheckResult.currentVersion = _currentVersion;

    BaseType_t taskCreated = xTaskCreatePinnedToCore(
        _checkUpdateTaskRunner,    // Function to implement the task
        "OtaCheckTask",          // Name of the task
        OTA_TASK_STACK_SIZE,     // Stack size in words
        this,                    // Task input parameter (OtaManager instance)
        OTA_TASK_PRIORITY,       // Priority of the task
        &_checkTaskHandle,       // Task handle
        APP_CPU_NUM              // Pin to application core (core 1 for ESP32)
    );

    if (taskCreated != pdPASS) {
        Serial.println("OTA Error: Failed to create update check task.");
        _setUpdateStatus(UpdateStatus::State::IDLE, "Failed to start check task", 0);
        _checkTaskHandle = NULL;
        return false;
    }

    Serial.println("OTA Info: Update check task started.");
    return true;
}

// Helper function to perform HTTPS GET request
String OtaManager::_performHttpsRequest(const char* url, const char* rootCa) {
    WiFiClientSecure client;
    HTTPClient https;
    
    Serial.printf("OTA: Connecting to %s\n", url);
    client.setCACert(rootCa);
    client.setTimeout(10000); // 10 second timeout

    // Enable automatic redirect following
    https.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS); // Or HTTPC_FORCE_FOLLOW_REDIRECTS or true for older cores

    if (https.begin(client, url)) {
        https.addHeader("User-Agent", "ESP32-OTA-Client"); // GitHub requires User-Agent
        int httpCode = https.GET();

        if (httpCode > 0) {
            Serial.printf("OTA: HTTP GET successful, code: %d\n", httpCode);
            if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_MOVED_PERMANENTLY) {
                String payload = https.getString();
                https.end();
                return payload;
            } else {
                 Serial.printf("OTA Error: HTTP GET failed, code: %d\n", httpCode);
                // Include HTTP code in error message if possible
                 _setUpdateStatus(UpdateStatus::State::ERROR_HTTP_CHECK, "HTTP Error: " + String(httpCode), 0);
            }
        } else {
            Serial.printf("OTA Error: HTTP GET failed, error: %s\n", https.errorToString(httpCode).c_str());
            _setUpdateStatus(UpdateStatus::State::ERROR_HTTP_CHECK, "Connection failed", 0);
        }
        https.end();
    } else {
        Serial.printf("OTA Error: Unable to connect to %s\n", url);
         _setUpdateStatus(UpdateStatus::State::ERROR_HTTP_CHECK, "Connection failed", 0);
    }

    return ""; // Return empty string on error
}

// Helper function to parse GitHub API response
UpdateInfo OtaManager::_parseGithubApiResponse(const String& jsonPayload) {
    UpdateInfo info;
    info.currentVersion = _currentVersion;

    if (jsonPayload.isEmpty()) {
        info.error = _currentStatus.message; // Use status message set by _performHttpsRequest
        return info;
    }

    // Increased JSON document size - GitHub API response can be large
    // Adjust based on actual response size observed during testing
    DynamicJsonDocument doc(8192);
    DeserializationError error = deserializeJson(doc, jsonPayload);

    if (error) {
        Serial.printf("OTA Error: deserializeJson() failed: %s\n", error.c_str());
        info.error = "JSON parsing failed";
        _setUpdateStatus(UpdateStatus::State::ERROR_JSON, info.error, 0);
        return info;
    }

    // Extract required fields
    const char* tagName = doc["tag_name"];
    const char* releaseNotes = doc["body"];
    JsonArray assets = doc["assets"].as<JsonArray>();

    if (!tagName) {
        info.error = "Tag name not found in response";
        _setUpdateStatus(UpdateStatus::State::ERROR_JSON, info.error, 0);
        return info;
    }

    info.availableVersion = String(tagName);
    info.releaseNotes = releaseNotes ? String(releaseNotes) : "";

    // Find the correct asset
    String downloadUrl = "";
    for (JsonObject asset : assets) {
        const char* assetName = asset["name"];
        if (assetName && strcmp(assetName, _firmwareAssetName) == 0) {
            const char* url = asset["browser_download_url"];
            if (url) {
                downloadUrl = String(url);
                break;
            }
        }
    }

    if (downloadUrl.isEmpty()) {
        info.error = "Firmware asset '" + String(_firmwareAssetName) + "' not found in release";
        _setUpdateStatus(UpdateStatus::State::ERROR_NO_ASSET, info.error, 0);
        return info;
    }

    info.downloadUrl = downloadUrl;

    // Compare versions (simple string comparison, assumes 'vX.Y.Z' format or similar)
    // TODO: Implement more robust semantic version comparison if needed
    if (info.availableVersion != info.currentVersion) {
        Serial.printf("OTA: Update available. Current: %s, Available: %s\n", info.currentVersion.c_str(), info.availableVersion.c_str());
        info.updateAvailable = true;
    } else {
        Serial.println("OTA: No update available (versions match)." );
        info.updateAvailable = false;
         _setUpdateStatus(UpdateStatus::State::IDLE, "No update available", 0);
    }

    return info;
}

// Begin update (placeholder implementation)
bool OtaManager::beginUpdate(const String& downloadUrl) {
    Serial.println("OTA DBG: beginUpdate - Entered function.");

    if (_checkTaskHandle != NULL){
        Serial.println("OTA Error: Cannot start update, a check task is still running.");
        _setUpdateStatus(UpdateStatus::State::IDLE, "Previous check task active",0);
        return false;
    }
    if (_updateTaskHandle != NULL) {
        Serial.println("OTA DBG: beginUpdate - Update already in progress (_updateTaskHandle not NULL).");
        // Optionally, could reflect this in status, but IDLE or current status is fine.
        // _setUpdateStatus(UpdateStatus::State::DOWNLOADING, "Update already in progress", _currentStatus.progress);
        return false; // Update task already running
    }

    // If an update check just completed and found an update, _currentStatus.status would be CHECKING_VERSION
    // We allow starting an update if IDLE or if a check has just confirmed an update is available.
    if (_currentStatus.status != UpdateStatus::State::IDLE && _currentStatus.status != UpdateStatus::State::CHECKING_VERSION) {
        Serial.printf("OTA DBG: beginUpdate - System not in a state to start update (current state: %d).\n", (int)_currentStatus.status);
        return false;
    }

    String urlToUse = downloadUrl;
    if (urlToUse.isEmpty()) {
        if (_lastCheckResult.updateAvailable && !_lastCheckResult.downloadUrl.isEmpty()) {
            urlToUse = _lastCheckResult.downloadUrl;
            Serial.println("OTA DBG: beginUpdate - Using download URL from last successful check.");
        } else {
            Serial.println("OTA Error: No download URL provided and no valid URL from last check.");
            _setUpdateStatus(UpdateStatus::State::IDLE, "Missing download URL", 0);
            return false;
        }
    }

    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("OTA DBG: beginUpdate - WiFi not connected. Cannot start update task.");
        _setUpdateStatus(UpdateStatus::State::ERROR_WIFI, "WiFi not connected for update", 0);
        _setUpdateStatus(UpdateStatus::State::IDLE, "Update failed: WiFi off", 0);
        return false;
    }

    Serial.println("OTA DBG: beginUpdate - About to call _ensureTimeSynced().");
    if (!_ensureTimeSynced()) {
        Serial.println("OTA DBG: beginUpdate - Time synchronization failed. Aborting update.");
        _setUpdateStatus(UpdateStatus::State::IDLE, "Time sync failed for update", 0);
        return false;
    }
    Serial.println("OTA DBG: beginUpdate - Time synchronized successfully.");

    Serial.printf("OTA DBG: beginUpdate - Preparing to start update from %s\n", urlToUse.c_str());
    _setUpdateStatus(UpdateStatus::State::DOWNLOADING, "Update process initiated...", 0);

    Serial.println("OTA DBG: beginUpdate - About to call xTaskCreatePinnedToCore for _updateTaskRunner.");
    BaseType_t taskCreated = xTaskCreatePinnedToCore(
        _updateTaskRunner,       // Function to implement the task
        "OtaUpdateTask",         // Name of the task
        OTA_TASK_STACK_SIZE,     // Stack size in words (ensure this is sufficient for HTTPS + Update lib)
        this,                    // Task input parameter (OtaManager instance)
        OTA_TASK_PRIORITY,       // Priority of the task
        &_updateTaskHandle,      // Task handle
        APP_CPU_NUM              // Pin to application core (core 1 for ESP32)
    );

    if (taskCreated != pdPASS) {
        Serial.printf("OTA DBG: beginUpdate - Failed to create update task. pdPASS = %d, taskCreated = %d\n", pdPASS, taskCreated);
        _setUpdateStatus(UpdateStatus::State::IDLE, "Failed to start update task", 0);
        _updateTaskHandle = NULL;
        return false;
    }

    Serial.println("OTA DBG: beginUpdate - Update task created successfully.");
    return true;
}

// Get status
UpdateStatus OtaManager::getStatus() {
    // In a real async implementation, this would return the latest status
    // updated by the background task.
    return _currentStatus;
}

// Get last check result
UpdateInfo OtaManager::getLastCheckResult() {
    return _lastCheckResult;
}

// Process loop (can be removed if tasks handle all async work)
void OtaManager::process() {
    // No longer strictly needed if tasks manage their lifecycle
}

// Private helper to set status and log
// Make sure this is declared in the header as well
void OtaManager::_setUpdateStatus(UpdateStatus::State state, const String& message, int progress) {
    _currentStatus.status = state;
    _currentStatus.message = message;
    if (progress >= 0) {
        _currentStatus.progress = progress;
    }
    Serial.printf("OTA Status: [%d] %s (%d%%)\n", (int)state, message.c_str(), _currentStatus.progress);
}

// Static task runner for performing the update
void OtaManager::_updateTaskRunner(void* pvParameters) {
    OtaManager* self = static_cast<OtaManager*>(pvParameters);
    String downloadUrl = self->_lastCheckResult.downloadUrl; // Assuming this is set by a prior check

    if (downloadUrl.isEmpty()) {
        Serial.println("OTA Task Error: Download URL is empty in _updateTaskRunner.");
        self->_setUpdateStatus(UpdateStatus::State::ERROR_HTTP_DOWNLOAD, "Internal error: Missing URL for update task", 0);
        self->_updateTaskHandle = NULL;
        vTaskDelete(NULL);
        return;
    }

    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("OTA Task Error: WiFi disconnected before download could start.");
        self->_setUpdateStatus(UpdateStatus::State::ERROR_WIFI, "WiFi disconnected", 0);
        self->_updateTaskHandle = NULL;
        vTaskDelete(NULL);
        return;
    }

    self->_setUpdateStatus(UpdateStatus::State::DOWNLOADING, "Starting download...", 0);

    WiFiClientSecure client;
    HTTPClient https;
    client.setCACert(self->_githubApiRootCa); // Use the same root CA
    client.setTimeout(15000); // Increased timeout for firmware download

    // Enable automatic redirect following
    https.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS); // Or HTTPC_FORCE_FOLLOW_REDIRECTS or true for older cores

    Serial.printf("OTA Task: Connecting to %s for firmware download\n", downloadUrl.c_str());

    if (!https.begin(client, downloadUrl)) {
        Serial.printf("OTA Task Error: Unable to connect to %s\n", downloadUrl.c_str());
        self->_setUpdateStatus(UpdateStatus::State::ERROR_HTTP_DOWNLOAD, "Connection failed for download", 0);
        self->_updateTaskHandle = NULL;
        vTaskDelete(NULL);
        return;
    }

    https.addHeader("User-Agent", "ESP32-OTA-Client");
    int httpCode = https.GET();

    if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_MOVED_PERMANENTLY) {
        int totalLength = https.getSize(); // Total size of the firmware
        if (totalLength <= 0) {
            Serial.println("OTA Task Error: Content length header missing or invalid.");
            self->_setUpdateStatus(UpdateStatus::State::ERROR_HTTP_DOWNLOAD, "Invalid content length", 0);
            https.end();
            self->_updateTaskHandle = NULL;
            vTaskDelete(NULL);
            return;
        }

        Serial.printf("OTA Task: Firmware size: %d bytes\n", totalLength);

        if (!Update.begin(totalLength)) {
            Serial.printf("OTA Task Error: Not enough space to begin OTA. Error: %u\n", Update.getError());
            self->_setUpdateStatus(UpdateStatus::State::ERROR_NO_SPACE, "Not enough space: " + String(Update.getError()), 0);
            https.end();
            self->_updateTaskHandle = NULL;
            vTaskDelete(NULL);
            return;
        }
        self->_setUpdateStatus(UpdateStatus::State::WRITING, "Flashing firmware...", 0);

        WiFiClient* stream = https.getStreamPtr();
        size_t written = 0;
        uint8_t buffer[1024];
        unsigned long lastProgressUpdate = millis();

        while (https.connected() && written < totalLength) {
            size_t len = stream->readBytes(buffer, sizeof(buffer));
            if (len > 0) {
                if (Update.write(buffer, len) != len) {
                    Serial.printf("OTA Task Error: Update.write failed. Error: %u\n", Update.getError());
                    self->_setUpdateStatus(UpdateStatus::State::ERROR_UPDATE_WRITE, "Write error: " + String(Update.getError()), (written * 100) / totalLength);
                    https.end();
                    Update.abort(); // Abort the update
                    self->_updateTaskHandle = NULL;
                    vTaskDelete(NULL);
                    return;
                }
                written += len;
                int progress = (written * 100) / totalLength;
                // Update progress not too frequently to avoid flooding logs/slowing down
                if (millis() - lastProgressUpdate > 1000 || progress == 100) { 
                    self->_setUpdateStatus(UpdateStatus::State::WRITING, "Flashing firmware...", progress);
                    lastProgressUpdate = millis();
                }
            } else if(len < 0) { // stream error
                 Serial.println("OTA Task Error: Stream read error during download.");
                 self->_setUpdateStatus(UpdateStatus::State::ERROR_HTTP_DOWNLOAD, "Stream read error", (written * 100) / totalLength);
                 https.end();
                 Update.abort();
                 self->_updateTaskHandle = NULL;
                 vTaskDelete(NULL);
                 return;
            }
            // Add a small delay to allow other tasks to run, esp. if stream is slow
            // vTaskDelay(pdMS_TO_TICKS(1)); // Commented out: Update.write is blocking; stream->readBytes has internal timeouts
        }

        if (written != totalLength) {
            Serial.printf("OTA Task Error: Firmware download incomplete. Expected %d, Got %d\n", totalLength, written);
            self->_setUpdateStatus(UpdateStatus::State::ERROR_HTTP_DOWNLOAD, "Download incomplete", (written * 100) / totalLength);
            https.end();
            Update.abort();
            self->_updateTaskHandle = NULL;
            vTaskDelete(NULL);
            return;
        }

        self->_setUpdateStatus(UpdateStatus::State::WRITING, "Finalizing update...", 100);
        if (!Update.end(true)) { // true to set the boot partition to the new one
            Serial.printf("OTA Task Error: Update.end failed. Error: %u\n", Update.getError());
            self->_setUpdateStatus(UpdateStatus::State::ERROR_UPDATE_END, "Commit error: " + String(Update.getError()), 100);
        } else {
            Serial.println("OTA Task: Update successful! Rebooting...");
            self->_setUpdateStatus(UpdateStatus::State::SUCCESS, "Update complete! Rebooting...", 100);
            ESP.restart(); // Reboot ESP
        }

    } else {
        Serial.printf("OTA Task Error: HTTP GET for firmware failed, code: %d, error: %s\n", httpCode, https.errorToString(httpCode).c_str());
        self->_setUpdateStatus(UpdateStatus::State::ERROR_HTTP_DOWNLOAD, "Download HTTP Error: " + String(httpCode), 0);
    }

    https.end();
    self->_updateTaskHandle = NULL; // Clear the task handle
    vTaskDelete(NULL); // Task deletes itself
}

// --- TODO: Implement these ---
// void OtaManager::_performUpdate(String url) { ... } // The function for the background task 