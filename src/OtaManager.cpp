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
#define OTA_TASK_PRIORITY 1     // Priority for OTA tasks - LOWERED FOR TESTING

// Constructor
OtaManager::OtaManager(const String& currentVersion, const String& repoOwner, const String& repoName)
    : _currentVersion(currentVersion),
      _repoOwner(repoOwner),
      _repoName(repoName),
      _firmwareAssetName("firmware.bin"), // Default firmware asset name
      _checkTaskHandle(NULL),
      _updateTaskHandle(NULL),
      _timeSynced(false) { // Initialize timeSynced
    heap_caps_malloc_extmem_enable(4096); 
    _currentStatus.status = UpdateStatus::State::IDLE;
    _currentStatus.message = "Idle";
    _currentStatus.progress = 0;
    _lastCheckResult.currentVersion = _currentVersion;

    _dataMutex = xSemaphoreCreateMutex();
    if (_dataMutex == NULL) {
        Serial.println("OTA Error: Failed to create data mutex!");
    }
}

// Static task runner for checking updates
void OtaManager::_checkUpdateTaskRunner(void* pvParameters) {
    OtaManager* self = static_cast<OtaManager*>(pvParameters);
    
    Serial.println("OTA DBG: _checkUpdateTaskRunner - Entered (EXTREMELY MINIMAL - IMMEDIATE EXIT TEST).");

    // Immediately set status to something simple and clear handle
    // Minimal mutex usage, just to update status and handle
    xSemaphoreTake(self->_dataMutex, portMAX_DELAY);
    self->_currentStatus.status = UpdateStatus::State::IDLE;
    self->_currentStatus.message = "OTA Check Task Ran (Minimal Exit Test)";
    self->_currentStatus.progress = 100;
    self->_checkTaskHandle = NULL; 
    xSemaphoreGive(self->_dataMutex);
    
    Serial.printf("OTA Status: [%d] %s (%d%%)\n", (int)UpdateStatus::State::IDLE, "OTA Check Task Ran (Minimal Exit Test)", 100);

    vTaskDelete(NULL); // Task deletes itself immediately

    // ALL PREVIOUS LOGIC BYPASSED:
    // UpdateInfo resultInfoLocal; 
    // resultInfoLocal.currentVersion = self->_currentVersion;
    // Serial.println("OTA DBG: _checkUpdateTaskRunner - Entered (BYPASSING _ensureTimeSynced, Simulating network/JSON work).");
    // if (!self->_ensureTimeSynced()) { ... }
    // Serial.println("OTA DBG: _checkUpdateTaskRunner - Simulating 3-second delay for HTTPS/JSON (NO TIME SYNC CALLED)...");
    // vTaskDelay(pdMS_TO_TICKS(3000)); // Simulate work
    // if (WiFi.status() != WL_CONNECTED) { ... } else { ... }
    // xSemaphoreTake(self->_dataMutex, portMAX_DELAY);
    // self->_lastCheckResult = resultInfoLocal; 
    // ... (status update logic based on resultInfoLocal) ...
    // xSemaphoreGive(self->_dataMutex);
    // Serial.printf("OTA Status: [%d] %s (%d%%)\n", (int)stateToLog, messageToLog.c_str(), progressToLog);
    // self->_checkTaskHandle = NULL; 
    // vTaskDelete(NULL); 
}

// Private helper to ensure time is synchronized via NTP
bool OtaManager::_ensureTimeSynced() {
    // Accessing _timeSynced, assuming it's only written by this function or constructor, 
    // so direct read is okay if called sequentially within a task.
    // If other tasks could modify _timeSynced, it would need protection.
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
    if (!getLocalTime(&timeinfo, 10000)) { 
        Serial.println("OTA Error: Failed to obtain time from NTP server.");
        // No need to set _timeSynced = false here, as it's already false.
        return false;
    }
    Serial.println("OTA Info: Time synchronized successfully.");
    Serial.print("Current time: ");
    Serial.println(asctime(&timeinfo));
    _timeSynced = true; // Set only on success
    return true;
}

// Initiate non-blocking check for updates
bool OtaManager::checkForUpdate() {
    xSemaphoreTake(_dataMutex, portMAX_DELAY);
    if (_checkTaskHandle != NULL) {
        xSemaphoreGive(_dataMutex);
        Serial.println("OTA Info: Update check already in progress.");
        return false; 
    }
    // Current status check is implicitly handled by _checkTaskHandle being NULL
    xSemaphoreGive(_dataMutex);


    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("OTA Error: WiFi not connected. Cannot start update check task.");
        // Set status directly here as task won't run
        _setUpdateStatus(UpdateStatus::State::ERROR_WIFI, "WiFi not connected for check", 0);
        
        xSemaphoreTake(_dataMutex, portMAX_DELAY);
        _lastCheckResult = UpdateInfo();
        _lastCheckResult.currentVersion = _currentVersion;
        _lastCheckResult.error = "WiFi not connected for check";
        _lastCheckResult.updateAvailable = false;
        xSemaphoreGive(_dataMutex);
        
        // Transition to IDLE after setting error, so UI doesn't get stuck on "Checking..."
        // _setUpdateStatus(UpdateStatus::State::IDLE, "Check failed: WiFi off", 0); // _setUpdateStatus logs
        return false;
    }

    // Set status before creating task
    _setUpdateStatus(UpdateStatus::State::CHECKING_VERSION, "Checking for updates...", 0);
    
    xSemaphoreTake(_dataMutex, portMAX_DELAY);
    _lastCheckResult = UpdateInfo(); // Clear previous results
    _lastCheckResult.currentVersion = _currentVersion;
    xSemaphoreGive(_dataMutex);

    BaseType_t taskCreated = xTaskCreatePinnedToCore(
        _checkUpdateTaskRunner,    
        "OtaCheckTask",          
        OTA_TASK_STACK_SIZE,     
        this,                    
        OTA_TASK_PRIORITY,       
        &_checkTaskHandle,       
        APP_CPU_NUM              
    );

    if (taskCreated != pdPASS) {
        Serial.println("OTA Error: Failed to create update check task.");
        _setUpdateStatus(UpdateStatus::State::IDLE, "Failed to start check task", 0);
        // Ensure _checkTaskHandle is NULL if creation failed (though it should be assigned by xTaskCreate)
        xSemaphoreTake(_dataMutex, portMAX_DELAY);
        _checkTaskHandle = NULL; 
        xSemaphoreGive(_dataMutex);
        return false;
    }

    Serial.println("OTA Info: Update check task started.");
    return true;
}

// Helper function to perform HTTPS GET request
String OtaManager::_performHttpsRequest(const char* url, const char* rootCa) {
    WiFiClientSecure client;
    HTTPClient https;
    String payload = ""; // Initialize payload
    
    Serial.printf("OTA: Connecting to %s\n", url);
    client.setCACert(rootCa);
    client.setTimeout(10000); 

    https.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);

    if (https.begin(client, url)) {
        https.addHeader("User-Agent", "ESP32-OTA-Client"); 
        int httpCode = https.GET();

        if (httpCode > 0) {
            Serial.printf("OTA: HTTP GET successful, code: %d\n", httpCode);
            if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_MOVED_PERMANENTLY) {
                payload = https.getString();
            } else {
                 Serial.printf("OTA Error: HTTP GET failed, code: %d\n", httpCode);
                 // _setUpdateStatus is called by the task runner based on error in UpdateInfo
            }
        } else {
            Serial.printf("OTA Error: HTTP GET failed, error: %s\n", https.errorToString(httpCode).c_str());
            // _setUpdateStatus called by task runner
        }
        https.end(); // Ensure end() is called if begin() was successful
    } else {
        Serial.printf("OTA Error: Unable to connect to %s\n", url);
        // _setUpdateStatus called by task runner
    }

    return payload; 
}

// Helper function to parse GitHub API response
UpdateInfo OtaManager::_parseGithubApiResponse(const String& jsonPayload) {
    UpdateInfo info; // Local copy
    xSemaphoreTake(_dataMutex, portMAX_DELAY);
    info.currentVersion = _currentVersion; // Get current version under mutex
    xSemaphoreGive(_dataMutex);


    if (jsonPayload.isEmpty()) {
        // If payload is empty, an error likely occurred in _performHttpsRequest.
        // The status message from that error will be more informative.
        // We rely on the task runner to have called _setUpdateStatus for HTTP errors.
        // So, here, just set the error field in info.
        info.error = "HTTP request failed or returned empty payload"; 
        // _setUpdateStatus(UpdateStatus::State::ERROR_HTTP_CHECK, info.error, 0); // Avoid calling from here, let task runner handle final status.
        return info;
    }

    DynamicJsonDocument doc(8192); 
    DeserializationError error = deserializeJson(doc, jsonPayload);

    if (error) {
        Serial.printf("OTA Error: deserializeJson() failed: %s\n", error.c_str());
        info.error = "JSON parsing failed";
        // _setUpdateStatus(UpdateStatus::State::ERROR_JSON, info.error, 0); // Let task runner set final status.
        return info;
    }

    const char* tagName = doc["tag_name"];
    const char* releaseNotes = doc["body"];
    JsonArray assets = doc["assets"].as<JsonArray>();

    if (!tagName) {
        info.error = "Tag name not found in response";
        // _setUpdateStatus(UpdateStatus::State::ERROR_JSON, info.error, 0);
        return info;
    }

    info.availableVersion = String(tagName);
    info.releaseNotes = releaseNotes ? String(releaseNotes) : "";

    String downloadUrl = "";
    // Access _firmwareAssetName safely under mutex if it could change. Assuming it's fixed after construction.
    // String firmwareAssetNameToFind = _firmwareAssetName; 
    for (JsonObject asset : assets) {
        const char* assetName = asset["name"];
        if (assetName && strcmp(assetName, _firmwareAssetName.c_str()) == 0) {
            const char* url_val = asset["browser_download_url"]; // Renamed to avoid conflict with outer 'url'
            if (url_val) {
                downloadUrl = String(url_val);
                break;
            }
        }
    }

    if (downloadUrl.isEmpty()) {
        info.error = "Firmware asset '" + _firmwareAssetName + "' not found in release";
        // _setUpdateStatus(UpdateStatus::State::ERROR_NO_ASSET, info.error, 0);
        return info;
    }

    info.downloadUrl = downloadUrl;

    if (info.availableVersion != info.currentVersion) {
        Serial.printf("OTA: Update available. Current: %s, Available: %s\n", info.currentVersion.c_str(), info.availableVersion.c_str());
        info.updateAvailable = true;
    } else {
        Serial.println("OTA: No update available (versions match)." );
        info.updateAvailable = false;
        // If no update, the task runner will set status to IDLE.
        // _setUpdateStatus(UpdateStatus::State::IDLE, "No update available", 0); 
    }

    return info;
}

// Begin update
bool OtaManager::beginUpdate(const String& downloadUrl_param) {
    Serial.println("OTA DBG: beginUpdate - Entered function.");

    xSemaphoreTake(_dataMutex, portMAX_DELAY);
    if (_checkTaskHandle != NULL){
        xSemaphoreGive(_dataMutex);
        Serial.println("OTA Error: Cannot start update, a check task is still running.");
        _setUpdateStatus(UpdateStatus::State::IDLE, "Previous check task active",0); // Ensure status is IDLE if blocked
        return false;
    }
    if (_updateTaskHandle != NULL) {
        xSemaphoreGive(_dataMutex);
        Serial.println("OTA DBG: beginUpdate - Update already in progress (_updateTaskHandle not NULL).");
        return false; 
    }

    UpdateStatus::State current_state_local = _currentStatus.status;
    String last_check_download_url = _lastCheckResult.downloadUrl;
    bool last_check_update_available = _lastCheckResult.updateAvailable;
    xSemaphoreGive(_dataMutex);


    if (current_state_local != UpdateStatus::State::IDLE && current_state_local != UpdateStatus::State::CHECKING_VERSION) {
        Serial.printf("OTA DBG: beginUpdate - System not in a state to start update (current state: %d).\n", (int)current_state_local);
        // If in an error state from a previous check, allow starting if user insists.
        // But if it's DOWNLOADING/WRITING etc., then disallow.
        // For now, this check is fine.
        return false;
    }

    String urlToUse = downloadUrl_param;
    if (urlToUse.isEmpty()) {
        if (last_check_update_available && !last_check_download_url.isEmpty()) {
            urlToUse = last_check_download_url;
            Serial.println("OTA DBG: beginUpdate - Using download URL from last successful check.");
        } else {
            Serial.println("OTA Error: No download URL provided and no valid URL from last check.");
            _setUpdateStatus(UpdateStatus::State::IDLE, "Missing download URL for update", 0);
            return false;
        }
    }

    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("OTA DBG: beginUpdate - WiFi not connected. Cannot start update task.");
        _setUpdateStatus(UpdateStatus::State::ERROR_WIFI, "WiFi not connected for update", 0);
        // _setUpdateStatus(UpdateStatus::State::IDLE, "Update failed: WiFi off", 0); // Let error state persist
        return false;
    }
    
    // Store the URL to be used by the task, under mutex
    xSemaphoreTake(_dataMutex, portMAX_DELAY);
    _lastCheckResult.downloadUrl = urlToUse; // Ensure the task uses this specific URL
    xSemaphoreGive(_dataMutex);


    Serial.println("OTA DBG: beginUpdate - About to call _ensureTimeSynced().");
    if (!_ensureTimeSynced()) {
        Serial.println("OTA DBG: beginUpdate - Time synchronization failed. Aborting update.");
        _setUpdateStatus(UpdateStatus::State::IDLE, "Time sync failed for update", 0); // Revert to IDLE
        return false;
    }
    Serial.println("OTA DBG: beginUpdate - Time synchronized successfully.");

    Serial.printf("OTA DBG: beginUpdate - Preparing to start update from %s\n", urlToUse.c_str());
    _setUpdateStatus(UpdateStatus::State::DOWNLOADING, "Update process initiated...", 0);
    
    BaseType_t taskCreated = xTaskCreatePinnedToCore(
        _updateTaskRunner,       
        "OtaUpdateTask",         
        OTA_TASK_STACK_SIZE,     
        this,                    
        OTA_TASK_PRIORITY,       
        &_updateTaskHandle,      
        APP_CPU_NUM              
    );

    if (taskCreated != pdPASS) {
        Serial.printf("OTA DBG: beginUpdate - Failed to create update task. pdPASS = %d, taskCreated = %d\n", pdPASS, taskCreated);
        _setUpdateStatus(UpdateStatus::State::IDLE, "Failed to start update task", 0);
        xSemaphoreTake(_dataMutex, portMAX_DELAY);
        _updateTaskHandle = NULL;
        xSemaphoreGive(_dataMutex);
        return false;
    }

    Serial.println("OTA DBG: beginUpdate - Update task created successfully.");
    return true;
}

// Get status
UpdateStatus OtaManager::getStatus() {
    UpdateStatus tempStatus;
    xSemaphoreTake(_dataMutex, portMAX_DELAY);
    tempStatus = _currentStatus; 
    xSemaphoreGive(_dataMutex);
    return tempStatus;
}

// Get last check result
UpdateInfo OtaManager::getLastCheckResult() {
    UpdateInfo tempInfo;
    xSemaphoreTake(_dataMutex, portMAX_DELAY);
    tempInfo = _lastCheckResult; 
    xSemaphoreGive(_dataMutex);
    return tempInfo;
}

// Process loop (can be removed if tasks handle all async work)
void OtaManager::process() {
    // No longer strictly needed if tasks manage their lifecycle
}

// Private helper to set status and log
void OtaManager::_setUpdateStatus(UpdateStatus::State state, const String& message, int progress) {
    xSemaphoreTake(_dataMutex, portMAX_DELAY);
    _currentStatus.status = state;
    _currentStatus.message = message;
    if (progress >= 0) { // Allow progress to be set to -1 to indicate no change
        _currentStatus.progress = progress;
    }
    // Capture values for logging before releasing mutex
    UpdateStatus::State stateToLog = _currentStatus.status;
    String messageToLog = _currentStatus.message;       
    int progressToLog = _currentStatus.progress;
    xSemaphoreGive(_dataMutex);

    Serial.printf("OTA Status: [%d] %s (%d%%)\n", (int)stateToLog, messageToLog.c_str(), progressToLog);
}

// Static task runner for performing the update
void OtaManager::_updateTaskRunner(void* pvParameters) {
    OtaManager* self = static_cast<OtaManager*>(pvParameters);
    String downloadUrl_local; 

    xSemaphoreTake(self->_dataMutex, portMAX_DELAY);
    downloadUrl_local = self->_lastCheckResult.downloadUrl; 
    xSemaphoreGive(self->_dataMutex);

    if (downloadUrl_local.isEmpty()) {
        Serial.println("OTA Task Error: Download URL is empty in _updateTaskRunner.");
        self->_setUpdateStatus(UpdateStatus::State::ERROR_HTTP_DOWNLOAD, "Internal error: Missing URL for update task", 0);
        xSemaphoreTake(self->_dataMutex, portMAX_DELAY);
        self->_updateTaskHandle = NULL;
        xSemaphoreGive(self->_dataMutex);
        vTaskDelete(NULL);
        return;
    }

    // Ensure time is synchronized (already done in beginUpdate, but good for robustness if task is somehow restarted)
    // Serial.println("OTA Task: Ensuring time is synchronized for update task...");
    // if (!self->_ensureTimeSynced()) {
    //     Serial.println("OTA Task Error: Time synchronization failed in update task. Aborting.");
    //     self->_setUpdateStatus(UpdateStatus::State::IDLE, "Time sync failed in update task", 0);
    //     xSemaphoreTake(self->_dataMutex, portMAX_DELAY);
    //     self->_updateTaskHandle = NULL;      
    //     xSemaphoreGive(self->_dataMutex);
    //     vTaskDelete(NULL);                
    //     return;
    // }
    // Serial.println("OTA Task: Time synchronized for update task.");


    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("OTA Task Error: WiFi disconnected before download could start.");
        self->_setUpdateStatus(UpdateStatus::State::ERROR_WIFI, "WiFi disconnected during update", 0);
        xSemaphoreTake(self->_dataMutex, portMAX_DELAY);
        self->_updateTaskHandle = NULL;
        xSemaphoreGive(self->_dataMutex);
        vTaskDelete(NULL);
        return;
    }

    self->_setUpdateStatus(UpdateStatus::State::DOWNLOADING, "Starting download...", 0);

    WiFiClientSecure client;
    HTTPClient https;
    // Access _githubApiRootCa safely. Assuming it's const and fixed after construction.
    client.setCACert(self->_githubApiRootCa); 
    client.setTimeout(15000); 

    https.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);

    Serial.printf("OTA Task: Connecting to %s for firmware download\n", downloadUrl_local.c_str());

    if (!https.begin(client, downloadUrl_local.c_str())) { 
        Serial.printf("OTA Task Error: Unable to connect to %s\n", downloadUrl_local.c_str());
        self->_setUpdateStatus(UpdateStatus::State::ERROR_HTTP_DOWNLOAD, "Connection failed for download", 0);
        xSemaphoreTake(self->_dataMutex, portMAX_DELAY);
        self->_updateTaskHandle = NULL;
        xSemaphoreGive(self->_dataMutex);
        vTaskDelete(NULL);
        return;
    }

    https.addHeader("User-Agent", "ESP32-OTA-Client");
    int httpCode = https.GET();

    if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_MOVED_PERMANENTLY) {
        int totalLength = https.getSize(); 
        if (totalLength <= 0) {
            Serial.println("OTA Task Error: Content length header missing or invalid.");
            self->_setUpdateStatus(UpdateStatus::State::ERROR_HTTP_DOWNLOAD, "Invalid content length", 0);
            https.end();
            xSemaphoreTake(self->_dataMutex, portMAX_DELAY);
            self->_updateTaskHandle = NULL;
            xSemaphoreGive(self->_dataMutex);
            vTaskDelete(NULL);
            return;
        }

        Serial.printf("OTA Task: Firmware size: %d bytes\n", totalLength);

        if (!Update.begin(totalLength)) {
            Serial.printf("OTA Task Error: Not enough space to begin OTA. Error: %u\n", Update.getError());
            self->_setUpdateStatus(UpdateStatus::State::ERROR_NO_SPACE, "Not enough space: " + String(Update.getError()), 0);
            https.end();
            xSemaphoreTake(self->_dataMutex, portMAX_DELAY);
            self->_updateTaskHandle = NULL;
            xSemaphoreGive(self->_dataMutex);
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
                    Update.abort(); 
                    xSemaphoreTake(self->_dataMutex, portMAX_DELAY);
                    self->_updateTaskHandle = NULL;
                    xSemaphoreGive(self->_dataMutex);
                    vTaskDelete(NULL);
                    return;
                }
                written += len;
                int progress = (written * 100) / totalLength;
                
                if (millis() - lastProgressUpdate > 1000 || progress == 100) { 
                    self->_setUpdateStatus(UpdateStatus::State::WRITING, "Flashing firmware...", progress);
                    lastProgressUpdate = millis();
                }
            } else if(len < 0) { 
                 Serial.println("OTA Task Error: Stream read error during download.");
                 self->_setUpdateStatus(UpdateStatus::State::ERROR_HTTP_DOWNLOAD, "Stream read error", (written * 100) / totalLength);
                 https.end();
                 Update.abort();
                 xSemaphoreTake(self->_dataMutex, portMAX_DELAY);
                 self->_updateTaskHandle = NULL;
                 xSemaphoreGive(self->_dataMutex);
                 vTaskDelete(NULL);
                 return;
            }
        }

        if (written != totalLength) {
            Serial.printf("OTA Task Error: Firmware download incomplete. Expected %d, Got %d\n", totalLength, written);
            self->_setUpdateStatus(UpdateStatus::State::ERROR_HTTP_DOWNLOAD, "Download incomplete", (written * 100) / totalLength);
            https.end();
            Update.abort();
            xSemaphoreTake(self->_dataMutex, portMAX_DELAY);
            self->_updateTaskHandle = NULL;
            xSemaphoreGive(self->_dataMutex);
            vTaskDelete(NULL);
            return;
        }

        self->_setUpdateStatus(UpdateStatus::State::WRITING, "Finalizing update...", 100);
        if (!Update.end(true)) { 
            Serial.printf("OTA Task Error: Update.end failed. Error: %u\n", Update.getError());
            self->_setUpdateStatus(UpdateStatus::State::ERROR_UPDATE_END, "Commit error: " + String(Update.getError()), 100);
        } else {
            Serial.println("OTA Task: Update successful! Rebooting...");
            self->_setUpdateStatus(UpdateStatus::State::SUCCESS, "Update complete! Rebooting...", 100);
            ESP.restart(); 
        }

    } else {
        Serial.printf("OTA Task Error: HTTP GET for firmware failed, code: %d, error: %s\n", httpCode, https.errorToString(httpCode).c_str());
        self->_setUpdateStatus(UpdateStatus::State::ERROR_HTTP_DOWNLOAD, "Download HTTP Error: " + String(httpCode), 0);
    }

    https.end();
    xSemaphoreTake(self->_dataMutex, portMAX_DELAY);
    self->_updateTaskHandle = NULL; 
    xSemaphoreGive(self->_dataMutex);
    vTaskDelete(NULL); 
}

