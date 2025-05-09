#include "OtaManager.h"
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <Update.h> // For ESP32 Update functions
#include "esp_task_wdt.h"
#include <WiFi.h> // For WiFi.status() and WL_CONNECTED

// For heap_caps_malloc and esp_ptr_external_ram, ensure correct include if not already covered by Arduino.h/ESP-IDF basics
// #include "esp_heap_caps.h" // Already in OtaManager.h but good to be mindful

#if ARDUINOJSON_VERSION_MAJOR >= 6 && ARDUINOJSON_VERSION_MINOR >= 18 
    #define OTAMANAGER_ARDUINOJSON_ENABLE_PSRAM ARDUINOJSON_ENABLE_PSRAM
#else
    #define OTAMANAGER_ARDUINOJSON_ENABLE_PSRAM 0 // Older versions might not have this macro defined
#endif

// Define BOARD_HAS_PSRAM in your platformio.ini or build flags if not auto-defined
// For example: build_flags = -DBOARD_HAS_PSRAM
// Or check for a specific board macro if your framework provides one.
#ifndef BOARD_HAS_PSRAM
    // Attempt to infer from common ESP32 SDK configs if not explicitly defined by build system
    #if CONFIG_ESP32_SPIRAM_SUPPORT || CONFIG_SPIRAM_SUPPORT // Common SDK config names
        #define BOARD_HAS_PSRAM 1
    #else
        #define BOARD_HAS_PSRAM 0
    #endif
#endif

#if OTAMANAGER_ARDUINOJSON_ENABLE_PSRAM && BOARD_HAS_PSRAM
struct PsramAllocator {
    // Check if PSRAM is available and initialized.
    // This function can be called once at setup or less frequently.
    static bool psramAvailable() {
        return heap_caps_get_total_size(MALLOC_CAP_SPIRAM) > 0;
    }

    void* allocate(size_t size) {
        if (psramAvailable()) {
            // Serial.printf("PsramAllocator: Attempting to allocate %u bytes from PSRAM\n", size);
            void* p = heap_caps_malloc(size, MALLOC_CAP_SPIRAM);
            // if (!p) Serial.println("PsramAllocator: heap_caps_malloc failed!");
            return p;
        }
        // Serial.printf("PsramAllocator: PSRAM not available, falling back to internal RAM for %u bytes\n", size);
        return malloc(size); // Fallback to internal RAM
    }

    void deallocate(void* pointer) {
        if (!pointer) return;
        if (esp_ptr_external_ram(pointer)) {
            // Serial.println("PsramAllocator: Deallocating from PSRAM");
            heap_caps_free(pointer);
        } else {
            // Serial.println("PsramAllocator: Deallocating from internal RAM");
            free(pointer);
        }
    }

    void* reallocate(void* ptr, size_t new_size) {
        if (esp_ptr_external_ram(ptr)) {
            // Serial.printf("PsramAllocator: Reallocating from PSRAM, new size %u\n", new_size);
            return heap_caps_realloc(ptr, new_size, MALLOC_CAP_SPIRAM);
        } else {
            // Serial.printf("PsramAllocator: Reallocating from internal RAM, new size %u\n", new_size);
            return realloc(ptr, new_size);
        }
    }
};
#endif

// Constructor
OtaManager::OtaManager(const String& currentVersion, const String& repoOwner, const String& repoName)
    : _currentVersion(currentVersion),
      _repoOwner(repoOwner),
      _repoName(repoName),
      _checkTaskHandle(NULL),
      _updateTaskHandle(NULL),
      _timeSynced(false) { // Initialize _timeSynced
    _currentStatus.status = UpdateStatus::State::IDLE;
    _currentStatus.message = "Idle";
    _currentStatus.progress = 0;
    _lastCheckResult.updateAvailable = false;
    _lastCheckResult.currentVersion = _currentVersion;

    _dataMutex = xSemaphoreCreateMutex();
    if (_dataMutex == NULL) {
        Serial.println("Error: Failed to create OtaManager data mutex!");
        // Handle error appropriately - perhaps a critical system halt or error state
    }
}

// Public methods
bool OtaManager::checkForUpdate() {
    if (_dataMutex) {
        if (xSemaphoreTake(_dataMutex, (TickType_t)10) == pdTRUE) { // Try to take mutex with short timeout
            if (_currentStatus.status == UpdateStatus::State::CHECKING_VERSION || 
                _currentStatus.status == UpdateStatus::State::DOWNLOADING || 
                _currentStatus.status == UpdateStatus::State::WRITING) {
                Serial.println("OtaManager: Check or update already in progress.");
                xSemaphoreGive(_dataMutex);
                return false; // Already checking or updating
            }
            // Minimal state to set before potentially long NTP sync
            _currentStatus.status = UpdateStatus::State::CHECKING_VERSION;
            _currentStatus.message = "Initializing update check...";
            _currentStatus.progress = 0;
            _lastCheckResult.error = ""; // Clear previous error before new check cycle
            _lastCheckResult.updateAvailable = false;
            _lastCheckResult.availableVersion = "";
            _lastCheckResult.downloadUrl = "";
            _lastCheckResult.releaseNotes = "";
            // Log this initial state change (moved from _setUpdateStatus for this specific case)
            Serial.printf("OtaManager Status: [%d] %s (%d%%)\n", static_cast<int>(_currentStatus.status), _currentStatus.message.c_str(), _currentStatus.progress);
            xSemaphoreGive(_dataMutex);
        } else {
            Serial.println("OtaManager: checkForUpdate could not acquire mutex to start check.");
            return false; // Could not get mutex, effectively busy
        }
    } else {
        Serial.println("OtaManager: _dataMutex is NULL in checkForUpdate.");
        return false; // Critical error
    }

    // Create a task to handle the update check. 
    // The task will be responsible for NTP sync and then the actual check.
    if (xTaskCreatePinnedToCore(
            _checkUpdateTaskRunner,   /* Task function */
            "otaCheckTask",         /* Name of task */
            8192,                   /* Stack size of task */
            this,                   /* Parameter of the task */
            1,                      /* Priority of the task */
            &_checkTaskHandle,      /* Task handle to keep track of the task */
            0                       /* Core pin, 0 for PRO_CPU */
        ) != pdPASS) {
        Serial.println("OtaManager: Failed to create check update task.");
        if (_dataMutex) xSemaphoreTake(_dataMutex, portMAX_DELAY);
        _setUpdateStatus(UpdateStatus::State::IDLE, "Failed to start check task");
        _lastCheckResult.error = "Failed to start check task.";
        if (_dataMutex) xSemaphoreGive(_dataMutex);
        return false;
    }
    return true;
}

bool OtaManager::beginUpdate(const String& downloadUrl) {
    if (downloadUrl.isEmpty()) {
        _setUpdateStatus(UpdateStatus::State::ERROR_UPDATE_BEGIN, "Download URL is empty.");
        return false;
    }

    if (_dataMutex) xSemaphoreTake(_dataMutex, portMAX_DELAY);
    if (_currentStatus.status == UpdateStatus::State::DOWNLOADING || _currentStatus.status == UpdateStatus::State::WRITING) {
        Serial.println("OtaManager: Update already in progress.");
        if (_dataMutex) xSemaphoreGive(_dataMutex);
        return false; 
    }
    _setUpdateStatus(UpdateStatus::State::DOWNLOADING, "Starting update...", 0);
    if (_dataMutex) xSemaphoreGive(_dataMutex);

    // Create a task to handle the update process
    // Pass the download URL to the task. Need to copy it as the String object might go out of scope.
    char* urlCopy = strdup(downloadUrl.c_str()); 
    if (!urlCopy) {
        Serial.println("OtaManager: Failed to allocate memory for URL copy.");
        _setUpdateStatus(UpdateStatus::State::ERROR_UPDATE_BEGIN, "Memory allocation failed for URL.");
        return false;
    }

    if (xTaskCreatePinnedToCore(
            _updateTaskRunner,      /* Task function */
            "otaUpdateTask",        /* Name of task */
            12288,                  /* Stack size of task (increased for HTTPS and flashing) */
            (void*)urlCopy,         /* Parameter of the task (pass the copied URL) */
            2,                      /* Priority of the task (higher than check) */
            &_updateTaskHandle,     /* Task handle */
            0                       /* Core pin */
        ) != pdPASS) {
        Serial.println("OtaManager: Failed to create update task.");
        free(urlCopy); // Free the copied URL if task creation fails
        if (_dataMutex) xSemaphoreTake(_dataMutex, portMAX_DELAY);
        _setUpdateStatus(UpdateStatus::State::IDLE, "Failed to start update task");
        if (_dataMutex) xSemaphoreGive(_dataMutex);
        return false;
    }
    return true;
}

UpdateStatus OtaManager::getStatus() {
    UpdateStatus currentStatusCopy;
    if (_dataMutex) {
        if (xSemaphoreTake(_dataMutex, (TickType_t)0) == pdTRUE) {
            currentStatusCopy = _currentStatus;
            xSemaphoreGive(_dataMutex);
        } else {
            // Mutex is busy, return a status indicating this clearly
            currentStatusCopy.status = UpdateStatus::State::MUTEX_BUSY;
            currentStatusCopy.message = "OTA manager busy, status temporarily unavailable.";
            currentStatusCopy.progress = 0; // Or -1, indicate progress unknown
        }
    } else {
        // Mutex not initialized, critical error
        currentStatusCopy.status = UpdateStatus::State::ERROR_INTERNAL;
        currentStatusCopy.message = "OTA mutex error (not initialized).";
        currentStatusCopy.progress = 0;
    }
    return currentStatusCopy;
}

UpdateInfo OtaManager::getLastCheckResult() {
    UpdateInfo lastCheckResultCopy;
    if (_dataMutex) {
        if (xSemaphoreTake(_dataMutex, (TickType_t)0) == pdTRUE) {
            lastCheckResultCopy = _lastCheckResult;
            xSemaphoreGive(_dataMutex);
        } else {
            // Mutex is busy, return a status indicating this clearly
            // Initialize to safe defaults, indicating data is unavailable
            lastCheckResultCopy.updateAvailable = false;
            lastCheckResultCopy.currentVersion = _currentVersion; // Current version is known, not from shared state
            lastCheckResultCopy.availableVersion = "";
            lastCheckResultCopy.downloadUrl = "";
            lastCheckResultCopy.releaseNotes = "";
            lastCheckResultCopy.error = "OTA manager busy, check result temporarily unavailable.";
        }
    } else {
        // Mutex not initialized, critical error
        lastCheckResultCopy.currentVersion = _currentVersion;
        lastCheckResultCopy.error = "OTA mutex error (not initialized).";
    }
    return lastCheckResultCopy;
}

void OtaManager::process() {
    // This method could be used if not using tasks, or for tasks to yield/delay.
    // For now, tasks handle themselves.
}

// Private helper methods
void OtaManager::_setUpdateStatus(UpdateStatus::State state, const String& message, int progress) {
    // Local copies for logging after mutex release
    UpdateStatus::State localState = state;
    String localMessage = message;
    int localProgress = progress;
    int actualProgressForLogging = 0; // Will be updated if progress is valid

    if (_dataMutex) {
        if (xSemaphoreTake(_dataMutex, portMAX_DELAY) == pdTRUE) {
            _currentStatus.status = localState;
            _currentStatus.message = localMessage;
            if (localProgress != -1) { // Only update progress if a valid value is provided
                _currentStatus.progress = localProgress;
                actualProgressForLogging = localProgress;
            } else {
                actualProgressForLogging = _currentStatus.progress; // Log existing progress
            }
            xSemaphoreGive(_dataMutex);
        } else {
            // Failed to take mutex, critical. Log with passed-in values.
            // This situation should ideally not happen with portMAX_DELAY.
            Serial.printf("CRITICAL_ERROR: OtaManager::_setUpdateStatus failed to take mutex. Requested state: [%d] %s (%d%%)\n", 
                          static_cast<int>(localState), localMessage.c_str(), localProgress == -1 ? -1 : localProgress);
            return; // Skip logging below as status wasn't actually set
        }
    } else {
        Serial.printf("CRITICAL_ERROR: OtaManager::_setUpdateStatus: _dataMutex is NULL. Requested state: [%d] %s (%d%%)\n", 
                      static_cast<int>(localState), localMessage.c_str(), localProgress == -1 ? -1 : localProgress);
        return; // Skip logging below
    }

    // Perform logging after mutex has been released
    Serial.printf("OtaManager Status: [%d] %s (%d%%)\n", 
                  static_cast<int>(localState), localMessage.c_str(), actualProgressForLogging);
}

String OtaManager::_performHttpsRequest(const char* url, const char* rootCa) {
    String payload = "";
    HTTPClient http;

    Serial.printf("OtaManager: Performing HTTPS request to: %s\n", url);

    // Ensure WiFi is connected (basic check, real check might be more robust or external)
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("OtaManager: WiFi not connected for HTTPS request.");
        if (_dataMutex) xSemaphoreTake(_dataMutex, portMAX_DELAY);
        _lastCheckResult.error = "WiFi not connected.";
        if (_dataMutex) xSemaphoreGive(_dataMutex);
        _setUpdateStatus(UpdateStatus::State::ERROR_WIFI, "WiFi not connected");
        return "";
    }

    http.begin(url, rootCa);
    http.setConnectTimeout(10000); // 10 seconds
    http.setTimeout(20000); // 20 seconds for the whole request
    int httpCode = http.GET();

    if (httpCode > 0) {
        Serial.printf("OtaManager: HTTPS GET successful, code: %d\n", httpCode);
        if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_MOVED_PERMANENTLY) {
            payload = http.getString();
        } else {
            Serial.printf("OtaManager: HTTPS GET failed, error: %s\n", http.errorToString(httpCode).c_str());
            if (_dataMutex) xSemaphoreTake(_dataMutex, portMAX_DELAY);
            _lastCheckResult.error = "HTTP error: " + String(httpCode);
            if (_dataMutex) xSemaphoreGive(_dataMutex);
        }
    } else {
        Serial.printf("OtaManager: HTTPS GET failed, error: %s\n", http.errorToString(httpCode).c_str());
         if (_dataMutex) xSemaphoreTake(_dataMutex, portMAX_DELAY);
        _lastCheckResult.error = "HTTP connection failed: " + http.errorToString(httpCode);
        if (_dataMutex) xSemaphoreGive(_dataMutex);
    }

    http.end();
    return payload;
}

UpdateInfo OtaManager::_parseGithubApiResponse(const String& jsonPayload) {
    UpdateInfo info;
    info.currentVersion = _currentVersion;

#if OTAMANAGER_ARDUINOJSON_ENABLE_PSRAM && BOARD_HAS_PSRAM
    Serial.println("OtaManager: Using PsramAllocator for JSON parsing.");
    // Capacity can be larger when using PSRAM, e.g., 10KB or 20KB depending on expected payload size
    // Let's try 10KB first. Max GitHub API response for releases is 30 items, but we only parse the first.
    // However, release notes can be long.
    BasicJsonDocument<PsramAllocator> doc(10240); 
#else
    Serial.println("OtaManager: Using default allocator (internal RAM) for JSON parsing.");
    DynamicJsonDocument doc(1536); // Previous reduced size for internal RAM fallback
#endif

    DeserializationError error = deserializeJson(doc, jsonPayload);
    if (error) {
        Serial.print(F("OtaManager: deserializeJson() failed: "));
        Serial.println(error.c_str());
        info.error = "JSON parsing failed: " + String(error.c_str());
        return info;
    }

    if (!doc.is<JsonArray>() || doc.as<JsonArray>().size() == 0) {
        Serial.println(F("OtaManager: JSON is not an array or is empty. No release found?"));
        info.error = "No release information found in API response.";
        return info;
    }

    JsonObject latestRelease = doc.as<JsonArray>()[0];
    const char* tagName = latestRelease["tag_name"];
    
    if (tagName) {
        info.availableVersion = String(tagName);
        // Simple version comparison (e.g., "v0.1.1" > "v0.1.0")
        // Assumes semantic versioning like vX.Y.Z or X.Y.Z
        String current = _currentVersion;
        String available = info.availableVersion;
        // Remove 'v' prefix if present for comparison
        if (current.startsWith("v")) current.remove(0, 1);
        if (available.startsWith("v")) available.remove(0, 1);

        if (available.compareTo(current) > 0) { // lexicographical comparison, good for SemVer if numbers are same width or major/minor/patch logic is used
            info.updateAvailable = true;
            Serial.printf("OtaManager: Update available. Current: %s, Available: %s\n", _currentVersion.c_str(), info.availableVersion.c_str());

            JsonArray assets = latestRelease["assets"].as<JsonArray>();
            for (JsonObject asset : assets) {
                if (String(asset["name"].as<String>()) == _firmwareAssetName) {
                    info.downloadUrl = asset["browser_download_url"].as<String>();
                    break;
                }
            }
            if (info.downloadUrl.isEmpty()) {
                 Serial.printf("OtaManager: Firmware asset '%s' not found in latest release.\n", _firmwareAssetName.c_str());
                 info.error = "Firmware asset not found.";
                 info.updateAvailable = false; // Cannot update if no download URL
            }
            const char* body = latestRelease["body"];
            if (body) {
                info.releaseNotes = String(body);
            }
        } else {
            Serial.printf("OtaManager: Firmware is up to date. Current: %s, Latest: %s\n", _currentVersion.c_str(), info.availableVersion.c_str());
        }
    } else {
        Serial.println(F("OtaManager: 'tag_name' not found in release JSON."));
        info.error = "Tag name not found in release info.";
    }
    return info;
}

bool OtaManager::_ensureTimeSynced() {
    if (_timeSynced) return true;

    Serial.println("OtaManager: Attempting to sync NTP time...");
    configTime(0, 0, "pool.ntp.org", "time.nist.gov"); // UTC, no DST offset, NTP servers
    
    time_t now = time(nullptr);
    int retries = 0;
    while (now < 8 * 3600 * 2) { // Check if time is reasonably after epoch (1 Jan 1970)
        delay(500);
        now = time(nullptr);
        retries++;
        if (retries > 20) { // Approx 10 seconds timeout
            Serial.println("OtaManager: NTP time sync failed after retries.");
            return false;
        }
    }
    struct tm timeinfo;
    gmtime_r(&now, &timeinfo);
    Serial.printf("OtaManager: NTP time synced: %s", asctime(&timeinfo));
    _timeSynced = true;
    return true;
}

// Static task runners
void OtaManager::_checkUpdateTaskRunner(void* pvParameters) {
    Serial.println("DEBUG: _checkUpdateTaskRunner started.");
    OtaManager* self = static_cast<OtaManager*>(pvParameters);
    esp_task_wdt_config_t twdt_config_check = {
        .timeout_ms = 30000, // 30 seconds
        .idle_core_mask = (1 << CONFIG_FREERTOS_NUMBER_OF_CORES) - 1,
        .trigger_panic = true
    };
    ESP_ERROR_CHECK(esp_task_wdt_init(&twdt_config_check));
    ESP_ERROR_CHECK(esp_task_wdt_add(NULL)); // Add current task to WDT

    // Perform NTP Sync first
    if (!self->_ensureTimeSynced()) {
        // NTP failed. Update status and terminate task.
        UpdateStatus::State ntpErrorState = UpdateStatus::State::ERROR_HTTP_CHECK; // Or a more specific NTP error state if you add one
        String ntpErrorMessage = "NTP time sync failed. Cannot check for updates.";
        
        if (self->_dataMutex) {
            if (xSemaphoreTake(self->_dataMutex, portMAX_DELAY) == pdTRUE) {
                self->_currentStatus.status = ntpErrorState;
                self->_currentStatus.message = ntpErrorMessage;
                self->_lastCheckResult.error = ntpErrorMessage;
                self->_checkTaskHandle = NULL; // Mark task as complete (failed)
                xSemaphoreGive(self->_dataMutex);
                Serial.printf("OtaManager Status Update (NTP Fail in _checkUpdateTaskRunner): [%d] %s\n", 
                              static_cast<int>(ntpErrorState), ntpErrorMessage.c_str());
            } else {
                Serial.println("ERROR: _checkUpdateTaskRunner (NTP Fail) failed to take mutex!");
                self->_checkTaskHandle = NULL;
            }
        }
        esp_task_wdt_delete(NULL);
        vTaskDelete(NULL);
        return; // Essential to exit the task here
    }

    // NTP Sync successful, now update status to indicate actual checking is starting
    // This uses the standard _setUpdateStatus which handles its own mutex and logging.
    self->_setUpdateStatus(UpdateStatus::State::CHECKING_VERSION, "Checking for updates (post-NTP)...", 0);

    String apiUrl = "https://api.github.com/repos/" + self->_repoOwner + "/" + self->_repoName + "/releases";
    String jsonPayload = self->_performHttpsRequest(apiUrl.c_str(), self->_githubApiRootCa);
    
    UpdateInfo localCheckResult;
    UpdateStatus::State newOtaState = UpdateStatus::State::IDLE;
    String newOtaMessage = "";
    int newOtaProgress = 0;

    if (!jsonPayload.isEmpty()) {
        localCheckResult = self->_parseGithubApiResponse(jsonPayload);
    } else {
        // _performHttpsRequest might have set an error in self->_lastCheckResult via _setUpdateStatus or direct write.
        // We should prioritize that error if available, or set a generic one.
        bool errorAlreadySet = false;
        if (self->_dataMutex) { // Check if mutex is valid before trying to take
            if (xSemaphoreTake(self->_dataMutex, (TickType_t)10) == pdTRUE) { // Brief wait
                if (!self->_lastCheckResult.error.isEmpty()) {
                    localCheckResult.error = self->_lastCheckResult.error;
                    errorAlreadySet = true;
                }
                // Also check current status if it reflects an HTTP error from _performHttpsRequest
                if (self->_currentStatus.status == UpdateStatus::State::ERROR_HTTP_CHECK || self->_currentStatus.status == UpdateStatus::State::ERROR_WIFI) {
                    newOtaState = self->_currentStatus.status;
                    newOtaMessage = self->_currentStatus.message;
                    errorAlreadySet = true; // Indicates we should use this state
                }
                xSemaphoreGive(self->_dataMutex);
            }
        }
        if (!errorAlreadySet) {
            localCheckResult.error = "Failed to fetch release info (empty payload).";
            newOtaState = UpdateStatus::State::ERROR_HTTP_CHECK;
            newOtaMessage = "Failed to fetch release info";
        }
    }

    // Logic to determine newOtaState and newOtaMessage based on localCheckResult
    // This logic was previously inside the mutex lock and calling _setUpdateStatus
    if (newOtaState != UpdateStatus::State::ERROR_HTTP_CHECK && newOtaState != UpdateStatus::State::ERROR_WIFI) { // Only proceed if no HTTP error already determined
        if (!localCheckResult.error.isEmpty() && !localCheckResult.updateAvailable) {
            newOtaState = UpdateStatus::State::ERROR_JSON;
            newOtaMessage = "Error processing release: " + localCheckResult.error;
        } else if (!localCheckResult.updateAvailable) {
            newOtaState = UpdateStatus::State::IDLE;
            newOtaMessage = "Firmware is up to date.";
        } else { // Update is available
            newOtaState = UpdateStatus::State::IDLE; // Or CHECKING_VERSION if you want to signify data is ready before user acts
                                                      // Setting to IDLE implies check is done, UI can show "Update Available"
            newOtaMessage = "Update available: " + localCheckResult.availableVersion;
            newOtaProgress = 100; // Check complete
        }
    }

    // Now, update shared state under a single mutex lock
    if (self->_dataMutex) {
        if (xSemaphoreTake(self->_dataMutex, portMAX_DELAY) == pdTRUE) {
            self->_lastCheckResult = localCheckResult; // Store the latest check result
            self->_lastCheckResult.currentVersion = self->_currentVersion; // Ensure current version is part of the result
            
            self->_currentStatus.status = newOtaState;
            self->_currentStatus.message = newOtaMessage;
            if (newOtaProgress != -1) { // Keep this progress update logic consistent
                self->_currentStatus.progress = newOtaProgress;
            }
            
            self->_checkTaskHandle = NULL; // Mark task as complete
            xSemaphoreGive(self->_dataMutex);

            // Log *after* releasing the mutex
            Serial.printf("OtaManager Status Update (from _checkUpdateTaskRunner): [%d] %s (%d%%)\n", 
                          static_cast<int>(newOtaState), newOtaMessage.c_str(), newOtaProgress);
        } else {
            Serial.println("ERROR: _checkUpdateTaskRunner failed to take mutex to update status!");
            // Handle critical error, perhaps try to clean up task handle if possible, though state is inconsistent
            self->_checkTaskHandle = NULL; // Still try to mark as done to prevent re-triggering issues
        }
    }
    
    esp_task_wdt_delete(NULL); // Remove current task from WDT
    vTaskDelete(NULL);
}

void OtaManager::_updateTaskRunner(void* pvParameters) {
    OtaManager* self = static_cast<OtaManager*>(pvParameters);
    char* downloadUrlCStr = static_cast<char*>(pvParameters);
    String downloadUrl = String(downloadUrlCStr);
    free(downloadUrlCStr); // Free the copied URL string now that we have it in a String object

    esp_task_wdt_config_t twdt_config_update = {
        .timeout_ms = 120000, // 120 seconds
        .idle_core_mask = (1 << CONFIG_FREERTOS_NUMBER_OF_CORES) - 1,
        .trigger_panic = true
    };
    ESP_ERROR_CHECK(esp_task_wdt_init(&twdt_config_update));
    ESP_ERROR_CHECK(esp_task_wdt_add(NULL));    // Add current task to WDT

    HTTPClient http;
    Serial.printf("OtaManager: Starting firmware update from URL: %s\n", downloadUrl.c_str());
    self->_setUpdateStatus(UpdateStatus::State::DOWNLOADING, "Downloading firmware...", 0);

    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("OtaManager: WiFi not connected for firmware download.");
        self->_setUpdateStatus(UpdateStatus::State::ERROR_WIFI, "WiFi not connected for download");
        if (self->_dataMutex) xSemaphoreTake(self->_dataMutex, portMAX_DELAY);
        self->_updateTaskHandle = NULL;
        if (self->_dataMutex) xSemaphoreGive(self->_dataMutex);
        esp_task_wdt_delete(NULL);
        vTaskDelete(NULL);
        return;
    }
    
    http.begin(downloadUrl);
    http.setConnectTimeout(10000); // 10s
    http.setTimeout(180000);      // 3 minutes for download
    int httpCode = http.GET();
    
    if (httpCode == HTTP_CODE_OK) {
        int totalSize = http.getSize();
        if (totalSize <= 0) {
            Serial.println("OtaManager: Content length header missing or zero.");
            self->_setUpdateStatus(UpdateStatus::State::ERROR_HTTP_DOWNLOAD, "Invalid content length from server.");
             if (self->_dataMutex) xSemaphoreTake(self->_dataMutex, portMAX_DELAY);
            self->_updateTaskHandle = NULL;
            if (self->_dataMutex) xSemaphoreGive(self->_dataMutex);
            http.end();
            esp_task_wdt_delete(NULL);
            vTaskDelete(NULL);
            return;
        }

        if (!Update.begin(totalSize)) {
            Serial.printf("OtaManager: Not enough space to begin OTA update. Error: %u\n", Update.getError());
            self->_setUpdateStatus(UpdateStatus::State::ERROR_NO_SPACE, "Not enough space for update. Error: " + String(Update.errorString()));
            if (self->_dataMutex) xSemaphoreTake(self->_dataMutex, portMAX_DELAY);
            self->_updateTaskHandle = NULL;
            if (self->_dataMutex) xSemaphoreGive(self->_dataMutex);
            http.end();
            esp_task_wdt_delete(NULL);
            vTaskDelete(NULL);
            return;
        }
        self->_setUpdateStatus(UpdateStatus::State::WRITING, "Writing firmware...", 0);

        WiFiClient* stream = http.getStreamPtr();
        size_t written = 0;
        uint8_t buff[1460] = {0}; // TCP MSS typical size
        unsigned long lastProgressUpdate = millis();

        while (http.connected() && (written < totalSize)) {
            esp_task_wdt_reset(); // Reset WDT periodically during download/write
            size_t len = stream->available();
            if (len > 0) {
                int c = stream->readBytes(buff, ((len > sizeof(buff)) ? sizeof(buff) : len));
                if (Update.write(buff, c) != c) {
                    self->_setUpdateStatus(UpdateStatus::State::ERROR_UPDATE_WRITE, "Firmware write error: " + String(Update.errorString()));
                     if (self->_dataMutex) xSemaphoreTake(self->_dataMutex, portMAX_DELAY);
                    self->_updateTaskHandle = NULL;
                    if (self->_dataMutex) xSemaphoreGive(self->_dataMutex);
                    Update.abort();
                    http.end();
                    esp_task_wdt_delete(NULL);
                    vTaskDelete(NULL);
                    return;
                }
                written += c;
                int progress = (int)(((float)written / totalSize) * 100);
                // Update progress not too frequently to avoid flooding serial/logs
                if (progress > self->_currentStatus.progress && (millis() - lastProgressUpdate > 1000 || progress == 100)) {
                    self->_setUpdateStatus(UpdateStatus::State::WRITING, "Writing firmware...", progress);
                    lastProgressUpdate = millis();
                }
            }
            delay(1); // Small delay to allow other tasks and prevent tight loop if stream is slow
        }

        if (written != totalSize) {
             Serial.printf("OtaManager: Update failed. Bytes written: %d / %d\n", written, totalSize);
             self->_setUpdateStatus(UpdateStatus::State::ERROR_HTTP_DOWNLOAD, "Download incomplete.");
             Update.abort();
        } else if (!Update.end(true)) { // true to set the boot partition
            Serial.printf("OtaManager: Error occurred during Update.end(): %u\n", Update.getError());
            self->_setUpdateStatus(UpdateStatus::State::ERROR_UPDATE_END, "Finalizing update error: " + String(Update.errorString()));
            Update.abort(); // ensure abort is called
        } else {
            self->_setUpdateStatus(UpdateStatus::State::SUCCESS, "Update successful! Rebooting...", 100);
            Serial.println("OtaManager: Update successful. Rebooting...");
            delay(1000); // Give a moment for serial message to get out
            ESP.restart();
        }

    } else {
        Serial.printf("OtaManager: Firmware download HTTP error: %d - %s\n", httpCode, http.errorToString(httpCode).c_str());
        self->_setUpdateStatus(UpdateStatus::State::ERROR_HTTP_DOWNLOAD, "HTTP error during download: " + String(httpCode));
    }
    http.end();
    
    // If we reach here and haven't rebooted, it means an error occurred after starting or during HTTP connection.
    // Ensure task handle is cleared if not already done by an error path.
    if (self->_dataMutex) xSemaphoreTake(self->_dataMutex, portMAX_DELAY);
    if (self->_currentStatus.status != UpdateStatus::State::SUCCESS) { // Only set to IDLE if not successful
        if(self->_currentStatus.status == UpdateStatus::State::DOWNLOADING || self->_currentStatus.status == UpdateStatus::State::WRITING){
             // If it was in progress but failed and didn't set a specific error state above
             self->_setUpdateStatus(UpdateStatus::State::IDLE, "Update failed"); 
        }
    }
    self->_updateTaskHandle = NULL;
    if (self->_dataMutex) xSemaphoreGive(self->_dataMutex);

    esp_task_wdt_delete(NULL); // Remove current task from WDT
    vTaskDelete(NULL);
}
