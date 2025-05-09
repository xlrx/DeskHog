function showScreen(screenId) {
    const screens = ['config-screen', 'success-screen', 'error-screen'];
    screens.forEach(id => {
        document.getElementById(id).classList.add('hidden');
    });
    document.getElementById(screenId).classList.remove('hidden');
    
    // Update page title
    let title = "DeskHog Configuration";
    if (screenId === 'success-screen') {
        title = "Configuration Saved";
        startCountdown(); // Start countdown only if success screen is explicitly shown by an action result
        // The actual redirect/countdown might be better handled based on specific messages from /api/status
    } else if (screenId === 'error-screen') {
        title = "Configuration Error";
    }
    document.getElementById('page-title').textContent = title;
    
    // Start countdown if success screen
    if (screenId === 'success-screen') {
        startCountdown();
        document.getElementById('progress-bar').style.width = '100%';
    }
}

// Handle WiFi form submission
function saveWifiConfig() {
    const form = document.getElementById('wifi-form');
    const formData = new FormData(form);
    
    fetch('/api/actions/save-wifi', { // New endpoint
        method: 'POST',
        body: formData
    })
    .then(response => response.json())
    .then(data => {
        if (data.status === 'initiated') {
            // Optionally show a general "Processing..." message
            // Actual success/error will be handled by pollApiStatus
            console.log("Save WiFi initiated.");
            // Consider a small toast message here: "Saving WiFi..."
        } else {
            // Handle "busy" or other errors from the initiation request itself
            showScreen('error-screen'); 
            document.getElementById('error-message-text').textContent = data.message || "Failed to initiate WiFi save.";
        }
    })
    .catch(() => {
        showScreen('error-screen');
        document.getElementById('error-message-text').textContent = "Communication error trying to save WiFi.";
    });
    
    return false; // Prevent default form submission
}

// Handle device config form submission
function saveDeviceConfig() {
    const form = document.getElementById('device-form');
    const formData = new FormData(form);
    
    fetch('/api/actions/save-device-config', { // New endpoint
        method: 'POST',
        body: formData
    })
    .then(response => response.json())
    .then(data => {
        if (data.status === 'initiated') {
            console.log("Save device config initiated.");
            // showScreen('success-screen'); // No longer show success immediately
        } else {
            showScreen('error-screen');
            document.getElementById('error-message-text').textContent = data.message || "Failed to initiate device config save.";
        }
    })
    .catch(() => {
        showScreen('error-screen');
        document.getElementById('error-message-text').textContent = "Communication error saving device config.";
    });
    
    return false;
}

// Toggle API key visibility
function toggleApiKeyVisibility() {
    const apiKeyInput = document.getElementById('apiKey');
    apiKeyInput.type = apiKeyInput.type === 'password' ? 'text' : 'password';
}

// Add new insight
function addInsight() {
    const form = document.getElementById('insight-form');
    const formData = new FormData(form);
    
    fetch('/api/actions/save-insight', { // New endpoint
        method: 'POST',
        body: formData
    })
    .then(response => response.json())
    .then(data => {
        if (data.status === 'initiated') {
            console.log("Add insight initiated.");
            form.reset(); // Reset form on initiation, actual list update via poll
            // loadInsights(); // No longer call directly
        } else {
            showScreen('error-screen');
            document.getElementById('error-message-text').textContent = data.message || "Failed to initiate save insight.";
        }
    })
    .catch(() => {
        showScreen('error-screen');
        document.getElementById('error-message-text').textContent = "Communication error saving insight.";
    });
    
    return false;
}

// Delete insight
function deleteInsight(id) {
    if (!confirm('Are you sure you want to delete this insight?')) {
        return;
    }
    
    // For POST with form data (if backend expects it for delete)
    const formData = new FormData();
    formData.append('id', id);

    fetch('/api/actions/delete-insight', { // New endpoint
        method: 'POST',
        // headers: { 'Content-Type': 'application/json', }, // Keep if backend handles JSON body for this
        // body: JSON.stringify({ id: id })
        body: formData // Assuming backend (CaptivePortal.cpp requestAction) now expects form data for delete param
    })
    .then(response => response.json())
    .then(data => {
        if (data.status === 'initiated') {
            console.log("Delete insight initiated.");
            // loadInsights(); // No longer call directly
        } else {
            showScreen('error-screen');
            document.getElementById('error-message-text').textContent = data.message || "Failed to initiate delete insight.";
        }
    })
    .catch(() => {
        showScreen('error-screen');
        document.getElementById('error-message-text').textContent = "Communication error deleting insight.";
    });
}

// Load insights list - UI update part will be in pollApiStatus
function _updateInsightsListUI(insights) { // Renamed to indicate it's a UI updater
    const container = document.getElementById('insights-list');
    container.innerHTML = '';
    
    if (!insights || insights.length === 0) {
        container.innerHTML = '<p>No insights configured</p>';
        return;
    }
    
    const list = document.createElement('ul');
    list.className = 'insights-list';
    
    insights.forEach(insight => {
        const item = document.createElement('li');
        item.className = 'insight-item';
        item.innerHTML = `
            <button onclick="deleteInsight('${insight.id}')" class="button danger">Delete ${insight.title}</button>
        `;
        list.appendChild(item);
    });
    
    container.appendChild(list);
}

// Refresh network list - UI update part will be in pollApiStatus
function _updateNetworksListUI(networks) { // Renamed
    const select = document.getElementById('ssid');
    const currentVal = select.value;
    select.innerHTML = '<option value="">Select a network</option>';
    
    if (!networks || networks.length === 0) {
        select.innerHTML += '<option disabled>No networks found</option>';
        return;
    }
    
    networks.forEach(network => {
        const option = document.createElement('option');
        option.value = network.ssid;
        
        let label = network.ssid;
        if (network.rssi >= -50) label += ' (Excellent)';
        else if (network.rssi >= -60) label += ' (Good)';
        else if (network.rssi >= -70) label += ' (Fair)';
        else label += ' (Poor)';
        if (network.encrypted) label += ' 🔒';
        
        option.textContent = label;
        if (network.ssid === currentVal) {
            option.selected = true;
        }
        select.appendChild(option);
    });
}

function requestScanNetworks() {
    fetch('/api/actions/start-wifi-scan', { method: 'POST' })
        .then(response => response.json())
        .then(data => {
            if (data.status === 'initiated') {
                console.log("WiFi scan initiated.");
                // Optionally show "Scanning..." text near the dropdown
                document.getElementById('ssid').innerHTML = '<option>Scanning...</option>';
            } else {
                console.error("Failed to initiate WiFi scan: ", data.message);
                 document.getElementById('ssid').innerHTML = '<option>Scan failed to start.</option>';
            }
        })
        .catch(error => {
            console.error('Error requesting network scan:', error);
            document.getElementById('ssid').innerHTML = '<option value="">Error starting scan</option>';
        });
}

// Main function to poll /api/status and update UI
let lastProcessedAction = null; // To track last action for one-time messages like success/error screens
let lastProcessedActionMessage = "";
let initialDeviceConfigLoaded = false; // Flag to track initial device config load

function pollApiStatus() {
    fetch('/api/status')
        .then(response => response.json())
        .then(data => {
            // console.log('[API_STATUS]', data);

            // 1. Update Portal Action Status (generic messages, loading indicators)
            const portalStatus = data.portal;
            if (portalStatus) {
                const actionInProgressEl = document.getElementById('action-in-progress-message'); // Assume such an element exists for global status
                if (actionInProgressEl) {
                    if (portalStatus.action_in_progress && portalStatus.action_in_progress !== 'NONE') {
                        actionInProgressEl.textContent = `Processing: ${portalStatus.action_in_progress}...`;
                        actionInProgressEl.style.display = 'block';
                    } else {
                        actionInProgressEl.style.display = 'none';
                    }
                }

                // Handle completion of an action (e.g., show success/error screen once)
                if (portalStatus.last_action_completed && portalStatus.last_action_completed !== 'NONE') {
                    const completedActionKey = portalStatus.last_action_completed + '-' + portalStatus.last_action_message; // Simple key for uniqueness
                    if (lastProcessedAction !== completedActionKey) {
                        console.log(`Action completed: ${portalStatus.last_action_completed}, Status: ${portalStatus.last_action_status}, Msg: ${portalStatus.last_action_message}`);
                        if (portalStatus.last_action_status === 'SUCCESS') {
                            // Show success screen for specific actions if desired
                            if (['SAVE_WIFI', 'SAVE_DEVICE_CONFIG', 'SAVE_INSIGHT'].includes(portalStatus.last_action_completed)) {
                                showScreen('success-screen'); 
                            } else {
                                // Or a toast message for other successes
                                // e.g., showToast(portalStatus.last_action_message, 'success');
                            }
                        } else if (portalStatus.last_action_status === 'ERROR') {
                            showScreen('error-screen');
                            const errorMsgEl = document.getElementById('error-message-text');
                            if (errorMsgEl) errorMsgEl.textContent = portalStatus.last_action_message || "An unknown error occurred.";
                        }
                        lastProcessedAction = completedActionKey;
                        lastProcessedActionMessage = portalStatus.last_action_message;
                        
                        // Clear the message after a short delay or user interaction if it's a transient message not on success/error screen
                        // setTimeout(() => { if(lastProcessedAction === completedActionKey) lastProcessedAction = null; }, 10000);
                    }
                }
            }

            // 2. Update WiFi Info
            if (data.wifi) {
                _updateNetworksListUI(data.wifi.networks);
                const wifiStatusEl = document.getElementById('wifi-connection-status'); // Assume element exists
                if (wifiStatusEl) {
                    wifiStatusEl.textContent = data.wifi.is_connected ? `Connected to ${data.wifi.connected_ssid} (${data.wifi.ip_address})` : "Not Connected";
                }
            }

            // 3. Update Device Config Info
            if (data.device_config) {
                _updateDeviceConfigUI(data.device_config);
            }

            // 4. Update Insights List
            if (data.insights) {
                _updateInsightsListUI(data.insights);
            }

            // 5. Update OTA Firmware Info & UI State
            if (data.ota) {
                updateOtaUI(data.ota, data.portal);
            }

        })
        .catch(error => {
            console.error('Error polling /api/status:', error);
            // Display a global error message, e.g., "Lost connection to device"
            const actionInProgressEl = document.getElementById('action-in-progress-message');
            if(actionInProgressEl) {
                actionInProgressEl.textContent = 'Error fetching status from device. Check connection.';
                actionInProgressEl.style.display = 'block';
            }
        });
}

// Load current configuration - UI update part will be in pollApiStatus
function _updateDeviceConfigUI(config) { // Renamed
    if (!initialDeviceConfigLoaded) {
        if (config.team_id !== undefined) {
            document.getElementById('teamId').value = config.team_id;
        }
        // The API key from /api/status should be the display version (e.g., ********1234)
        // We only set it if it's the first load, otherwise user input might be overwritten.
        // The actual API key is sent on form submission, not from this display value.
        if (config.api_key_display !== undefined) { 
            document.getElementById('apiKey').value = config.api_key_display;
        }
        initialDeviceConfigLoaded = true;
    }
}

// Initialize page
document.addEventListener('DOMContentLoaded', function() {
    // Check if we need to show a specific screen based on URL hash
    // This might be less relevant if all feedback comes via /api/status poll
    const hash = window.location.hash.substr(1);
    if (hash && ['config-screen', 'success-screen', 'error-screen'].includes(hash)) {
        showScreen(hash);
    }
    
    // // Load current configuration - Now handled by pollApiStatus
    // loadCurrentConfig();
    // // Load insights list - Now handled by pollApiStatus
    // loadInsights();
    // // Populate the networks list - Now handled by pollApiStatus initial call & requestScanNetworks
    // refreshNetworks(); 

    // OTA Update functionality
    const checkUpdateBtn = document.getElementById('check-update-btn');
    const installUpdateBtn = document.getElementById('install-update-btn');

    if (checkUpdateBtn) {
        checkUpdateBtn.addEventListener('click', requestCheckFirmwareUpdate); // Changed to request check
    }
    if (installUpdateBtn) {
        installUpdateBtn.addEventListener('click', requestStartFirmwareUpdate); // Changed to request start
    }

    // Initial call to pollApiStatus to populate UI, then set interval
    pollApiStatus();
    setInterval(pollApiStatus, 5000); // Poll every 5 seconds

    // Add event listener to refresh networks button if it exists
    const refreshBtn = document.getElementById('refresh-networks-btn'); // Assuming a button with this ID
    if(refreshBtn) {
        refreshBtn.addEventListener('click', requestScanNetworks);
    }
});

// Removed: let otaPollingIntervalId = null;
// Removed: let pollingForCheckResult = false;

// Enum for OtaManager::UpdateStatus::State (mirror from C++)
// This helps in making the JS code more readable when checking status.
const OTA_STATUS_STATE = {
    IDLE: 0,
    CHECKING_VERSION: 1,
    DOWNLOADING: 2,
    WRITING: 3,
    SUCCESS: 4,
    ERROR_WIFI: 5,
    ERROR_HTTP_CHECK: 6,
    ERROR_HTTP_DOWNLOAD: 7,
    ERROR_JSON: 8,
    ERROR_UPDATE_BEGIN: 9,
    ERROR_UPDATE_WRITE: 10,
    ERROR_UPDATE_END: 11,
    ERROR_NO_ASSET: 12,
    ERROR_NO_SPACE: 13
};

function requestCheckFirmwareUpdate() {
    console.log("Requesting firmware update check...");
    const checkUpdateBtn = document.getElementById('check-update-btn');
    if(checkUpdateBtn) checkUpdateBtn.disabled = true;
    // UI updates for "checking..." will be handled by pollApiStatus based on action_in_progress

    fetch('/api/actions/check-ota-update', { method: 'POST' })
        .then(response => response.json())
        .then(data => {
            if (data.status === 'initiated') {
                console.log('Firmware check initiated.');
                // pollApiStatus will pick up the change and reflect status
            } else {
                console.error('Failed to initiate firmware check:', data.message);
                if(checkUpdateBtn) checkUpdateBtn.disabled = false;
                // Update UI to show initiation error - pollApiStatus might also do this via last_action_status
                const updateStatusTextEl = document.getElementById('update-status-text');
                if(updateStatusTextEl) updateStatusTextEl.textContent = `Error: ${data.message || 'Could not start check.'}`;
            }
        })
        .catch(error => {
            console.error('Error requesting firmware check:', error);
            if(checkUpdateBtn) checkUpdateBtn.disabled = false;
            const updateStatusTextEl = document.getElementById('update-status-text');
            if(updateStatusTextEl) updateStatusTextEl.textContent = 'Communication error during check request.';
        });
}

function requestStartFirmwareUpdate() {
    console.log("Requesting firmware update start...");
    const installUpdateBtn = document.getElementById('install-update-btn');
    const checkUpdateBtn = document.getElementById('check-update-btn');
    if(installUpdateBtn) installUpdateBtn.disabled = true;
    if(checkUpdateBtn) checkUpdateBtn.disabled = true;

    fetch('/api/actions/start-ota-update', { method: 'POST' })
        .then(response => response.json())
        .then(data => {
            if (data.status === 'initiated') {
                console.log('Firmware update process initiated.');
            } else {
                console.error('Failed to initiate firmware update:', data.message);
                if(installUpdateBtn) installUpdateBtn.disabled = false;
                if(checkUpdateBtn) checkUpdateBtn.disabled = false;
                const updateStatusTextEl = document.getElementById('update-status-text');
                if(updateStatusTextEl) updateStatusTextEl.textContent = `Error: ${data.message || 'Could not start update.'}`;
            }
        })
        .catch(error => {
            console.error('Error requesting firmware update start:', error);
            if(installUpdateBtn) installUpdateBtn.disabled = false;
            if(checkUpdateBtn) checkUpdateBtn.disabled = false;
            const updateStatusTextEl = document.getElementById('update-status-text');
            if(updateStatusTextEl) updateStatusTextEl.textContent = 'Communication error during update request.';
        });
}

// Helper function to update OTA UI based on status from /api/status
function updateOtaUI(otaData, portalData) {
    const currentVersionEl = document.getElementById('current-version');
    const availableVersionEl = document.getElementById('available-version');
    const releaseNotesEl = document.getElementById('release-notes');
    const updateAvailableSection = document.getElementById('update-available-section');
    const installUpdateBtn = document.getElementById('install-update-btn');
    const updateStatusTextEl = document.getElementById('update-status-text');
    const updateErrorEl = document.getElementById('update-error-message');
    const checkUpdateBtn = document.getElementById('check-update-btn');
    const updateProgressContainer = document.getElementById('update-progress-container');
    const updateProgressBar = document.getElementById('update-progress-bar');

    // New: Display portal's OTA action message first if present
    let portalOtaMessageDisplayed = false;
    if (portalData && portalData.portal_ota_action_message) {
        if (updateStatusTextEl) updateStatusTextEl.textContent = portalData.portal_ota_action_message;
        portalOtaMessageDisplayed = true;
        // Disable buttons if portal is actively processing an OTA request
        if (portalData.action_in_progress === 'CHECK_OTA_UPDATE' || portalData.action_in_progress === 'START_OTA_UPDATE') {
            if (checkUpdateBtn) checkUpdateBtn.disabled = true;
            if (installUpdateBtn) installUpdateBtn.disabled = true;
        } else {
            // If portal_ota_action_message indicates completion of portal dispatch, re-enable based on OtaManager status below
            // This 'else' means portal's direct involvement is done, buttons will be governed by otaData
        }
    }

    // Update displayed versions and notes - always try to update these from otaData
    if (currentVersionEl) currentVersionEl.textContent = otaData.current_firmware_version || 'N/A';
    if (availableVersionEl) availableVersionEl.textContent = otaData.update_available ? otaData.available_version : 'N/A';
    if (releaseNotesEl) releaseNotesEl.textContent = otaData.update_available ? (otaData.release_notes || 'No release notes.') : '';

    // Show/hide update available section and control install button based on otaData
    if (updateAvailableSection) updateAvailableSection.style.display = otaData.update_available ? 'block' : 'none';
    
    // Enable/disable install button: only if an update is available AND portal is not busy with an OTA action AND OtaManager is not busy
    if (installUpdateBtn) {
        installUpdateBtn.disabled = !otaData.update_available || 
                                  (portalData && (portalData.action_in_progress === 'START_OTA_UPDATE' || portalData.action_in_progress === 'CHECK_OTA_UPDATE')) ||
                                  otaData.status_code === OTA_STATUS_STATE.DOWNLOADING || 
                                  otaData.status_code === OTA_STATUS_STATE.WRITING;
    }
    
    // Enable/disable check update button: only if portal is not busy with an OTA action AND OtaManager is not busy
    if (checkUpdateBtn) {
        checkUpdateBtn.disabled = (portalData && (portalData.action_in_progress === 'CHECK_OTA_UPDATE' || portalData.action_in_progress === 'START_OTA_UPDATE')) ||
                                otaData.status_code === OTA_STATUS_STATE.CHECKING_VERSION || 
                                otaData.status_code === OTA_STATUS_STATE.DOWNLOADING || 
                                otaData.status_code === OTA_STATUS_STATE.WRITING;
    }

    // Update OtaManager status messages and progress bar, only if portal message wasn't primary
    if (!portalOtaMessageDisplayed || (portalData && portalData.portal_ota_action_message && portalData.portal_ota_action_message.includes("Successfully dispatched"))) {
        if (updateStatusTextEl) {
            // If portal message indicates successful dispatch, prefer OtaManager's message if available
            // otherwise, if no portal message, use OtaManager's message.
            let displayMessage = otaData.status_message || 'Idle';
            if (portalOtaMessageDisplayed && otaData.status_message && otaData.status_message !== "Idle") {
                // Prepend a note that this is OtaManager status if portal also had a successful dispatch message
                // updateStatusTextEl.textContent = "OtaManager: " + displayMessage;
                updateStatusTextEl.textContent = displayMessage; // Keep it simple, just show OtaManager status
            } else if (!portalOtaMessageDisplayed) {
                 updateStatusTextEl.textContent = displayMessage;
            }
            // If portalOtaMessageDisplayed was true but it was NOT a "Successfully dispatched" message (e.g. "pending execution"),
            // then the portal's message has already been set and we don't overwrite it here.
        }
    }
    
    if (updateErrorEl) {
        updateErrorEl.textContent = otaData.error_message || '';
    }

    if (otaData.status_code === OTA_STATUS_STATE.DOWNLOADING || otaData.status_code === OTA_STATUS_STATE.WRITING) {
        if (updateProgressContainer) updateProgressContainer.style.display = 'block';
        if (updateProgressBar) updateProgressBar.style.width = `${otaData.progress || 0}%`;
        // Buttons should already be disabled by the conditions above
    } else {
        // Hide progress bar unless successfully completed (then it might show 100% briefly)
        if (otaData.status_code !== OTA_STATUS_STATE.SUCCESS && updateProgressContainer) {
             // updateProgressContainer.style.display = 'none'; // Keep visible if there's a status message to show
        }
    }
    
    if (otaData.status_code === OTA_STATUS_STATE.SUCCESS) {
        if (updateStatusTextEl) updateStatusTextEl.textContent = otaData.status_message || 'Update successful! Device will reboot.';
        if (updateProgressBar) updateProgressBar.style.width = '100%';
        if (updateProgressContainer) updateProgressContainer.style.display = 'block';
        if (installUpdateBtn) installUpdateBtn.disabled = true;
        if (checkUpdateBtn) checkUpdateBtn.disabled = true;
    }
}

// Removed old pollUpdateStatus function as its logic is merged into pollApiStatus and updateOtaUI
/*
function pollUpdateStatus() {
    const currentVersionEl = document.getElementById('current-version');
// ... (old function content removed)
}
*/