function showScreen(screenId) {
    const screens = ['config-screen', 'success-screen', 'error-screen'];
    screens.forEach(id => {
        document.getElementById(id).classList.add('hidden');
    });
    document.getElementById(screenId).classList.remove('hidden');
    
    // Update page title
    let title = "DeskHog Configuration";
    if (screenId === 'success-screen') title = "Configuration Saved";
    else if (screenId === 'error-screen') title = "Configuration Error";
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
    
    fetch('/save-wifi', {
        method: 'POST',
        body: formData
    })
    .then(response => response.json())
    .then(data => {
        if (data.success) {
            showScreen('success-screen');
        } else {
            showScreen('error-screen');
        }
    })
    .catch(() => {
        showScreen('error-screen');
    });
    
    return false; // Prevent default form submission
}

// Handle device config form submission
function saveDeviceConfig() {
    const form = document.getElementById('device-form');
    const formData = new FormData(form);
    
    fetch('/save-device-config', {
        method: 'POST',
        body: formData
    })
    .then(response => response.json())
    .then(data => {
        if (data.success) {
            showScreen('success-screen');
        } else {
            showScreen('error-screen');
        }
    })
    .catch(() => {
        showScreen('error-screen');
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
    
    fetch('/save-insight', {
        method: 'POST',
        body: formData
    })
    .then(response => response.json())
    .then(data => {
        if (data.success) {
            form.reset();
            loadInsights();
        } else {
            showScreen('error-screen');
        }
    })
    .catch(() => {
        showScreen('error-screen');
    });
    
    return false;
}

// Delete insight
function deleteInsight(id) {
    if (!confirm('Are you sure you want to delete this insight?')) {
        return;
    }
    
    fetch('/delete-insight', {
        method: 'POST',
        headers: {
            'Content-Type': 'application/json',
        },
        body: JSON.stringify({ id: id })
    })
    .then(response => response.json())
    .then(data => {
        if (data.success) {
            loadInsights();
        } else {
            showScreen('error-screen');
        }
    })
    .catch(() => {
        showScreen('error-screen');
    });
}

// Load insights list
function loadInsights() {
    fetch('/get-insights')
    .then(response => response.json())
    .then(data => {
        const container = document.getElementById('insights-list');
        container.innerHTML = '';
        
        if (data.insights.length === 0) {
            container.innerHTML = '<p>No insights configured</p>';
            return;
        }
        
        const list = document.createElement('ul');
        list.className = 'insights-list';
        
        data.insights.forEach(insight => {
            const item = document.createElement('li');
            item.className = 'insight-item';
            item.innerHTML = `
                <button onclick="deleteInsight('${insight.id}')" class="button danger">Delete ${insight.title}</button>
            `;
            list.appendChild(item);
        });
        
        container.appendChild(list);
    })
    .catch(error => {
        console.error('Error loading insights:', error);
    });
}

// Refresh network list
function refreshNetworks() {
    fetch('/scan-networks')
    .then(response => response.json())
    .then(data => {
        const select = document.getElementById('ssid');
        select.innerHTML = '<option value="">Select a network</option>';
        
        if (!data.networks || data.networks.length === 0) {
            select.innerHTML += '<option disabled>No networks found</option>';
            return;
        }
        
        data.networks.forEach(network => {
            const option = document.createElement('option');
            option.value = network.ssid;
            
            let label = network.ssid;
            
            // Add signal strength indicator
            if (network.rssi >= -50) {
                label += ' (Excellent)';
            } else if (network.rssi >= -60) {
                label += ' (Good)';
            } else if (network.rssi >= -70) {
                label += ' (Fair)';
            } else {
                label += ' (Poor)';
            }
            
            // Add lock icon for encrypted networks
            if (network.encrypted) {
                label += ' 🔒';
            }
            
            option.textContent = label;
            select.appendChild(option);
        });
    })
    .catch(error => {
        console.error('Error refreshing networks:', error);
        const select = document.getElementById('ssid');
        select.innerHTML = '<option value="">Error loading networks</option>';
    });
}

// Start countdown on success screen
function startCountdown() {
    let seconds = 10;
    const countdownEl = document.getElementById('countdown');
    
    const interval = setInterval(() => {
        seconds--;
        countdownEl.textContent = seconds;
        
        if (seconds <= 0) {
            clearInterval(interval);
            window.location.href = '/';
        }
    }, 1000);
}

// Load current configuration
function loadCurrentConfig() {
    fetch('/get-device-config')
    .then(response => response.json())
    .then(data => {
        if (data.teamId !== undefined) {
            document.getElementById('teamId').value = data.teamId;
        }
        if (data.apiKey) {
            document.getElementById('apiKey').value = data.apiKey;
        }
    })
    .catch(error => {
        console.error('Error loading device config:', error);
    });
}

// Initialize page
document.addEventListener('DOMContentLoaded', function() {
    // Check if we need to show a specific screen based on URL hash
    const hash = window.location.hash.substr(1);
    if (hash && ['config-screen', 'success-screen', 'error-screen'].includes(hash)) {
        showScreen(hash);
    }
    
    // Load current configuration
    loadCurrentConfig();
    
    // Load insights list
    loadInsights();

    // Populate the networks list
    refreshNetworks();

    // OTA Update functionality
    const checkUpdateBtn = document.getElementById('check-update-btn');
    const installUpdateBtn = document.getElementById('install-update-btn');

    if (checkUpdateBtn) {
        checkUpdateBtn.addEventListener('click', checkFirmwareUpdate);
    }
    if (installUpdateBtn) {
        installUpdateBtn.addEventListener('click', startFirmwareUpdate);
    }

    // Initial check for firmware version on page load
    checkFirmwareUpdate(); 
});

let otaPollingIntervalId = null;
let pollingForCheckResult = false; // Flag to differentiate polling purpose

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

function checkFirmwareUpdate() {
    console.log("Checking for firmware updates...");
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

    if(updateStatusTextEl) updateStatusTextEl.textContent = 'Initiating check...';
    if(updateErrorEl) updateErrorEl.textContent = '';
    if(updateAvailableSection) updateAvailableSection.style.display = 'none';
    if(installUpdateBtn) installUpdateBtn.disabled = true;
    if(checkUpdateBtn) checkUpdateBtn.disabled = true; // Disable while initiating/polling check
    if(updateProgressContainer) updateProgressContainer.style.display = 'none';
    if(updateProgressBar) updateProgressBar.style.width = '0%';

    // Stop any previous polling
    if (otaPollingIntervalId) {
        clearInterval(otaPollingIntervalId);
        otaPollingIntervalId = null;
    }

    fetch('/check-update') // This now only initiates the check
        .then(response => response.json())
        .then(data => {
            if (currentVersionEl) currentVersionEl.textContent = data.current_firmware_version || 'N/A';
            if (updateStatusTextEl) updateStatusTextEl.textContent = data.initial_status_message || 'Checking...';

            if (data.check_initiated) {
                console.log('Update check initiated. Polling for status...');
                pollingForCheckResult = true; // Set flag
                otaPollingIntervalId = setInterval(pollUpdateStatus, 2000); // Poll every 2 seconds
                // checkUpdateBtn remains disabled while polling
            } else {
                console.error('Failed to initiate update check:', data.initial_status_message);
                if(updateErrorEl) updateErrorEl.textContent = `Error: ${data.initial_status_message || 'Could not start update check.'}`;
                if(updateStatusTextEl) updateStatusTextEl.textContent = 'Check failed to start.';
                if(checkUpdateBtn) checkUpdateBtn.disabled = false; // Re-enable if initiation failed
            }
        })
        .catch(error => {
            console.error('Failed to communicate with device to check for updates:', error);
            if (currentVersionEl) currentVersionEl.textContent = 'Error';
            if (availableVersionEl) availableVersionEl.textContent = 'Error';
            if(updateErrorEl) updateErrorEl.textContent = 'Communication error during check initiation.';
            if(updateStatusTextEl) updateStatusTextEl.textContent = 'Error initiating check.';
            if(checkUpdateBtn) checkUpdateBtn.disabled = false; // Re-enable on communication error
        });
}

function startFirmwareUpdate() {
    console.log("Starting firmware update...");
    const installUpdateBtn = document.getElementById('install-update-btn');
    const checkUpdateBtn = document.getElementById('check-update-btn');
    const updateStatusTextEl = document.getElementById('update-status-text');
    const updateErrorEl = document.getElementById('update-error-message');
    const updateProgressContainer = document.getElementById('update-progress-container');
    const updateProgressBar = document.getElementById('update-progress-bar');

    if (installUpdateBtn) installUpdateBtn.disabled = true;
    if (checkUpdateBtn) checkUpdateBtn.disabled = true; // Disable check button during update
    if (updateErrorEl) updateErrorEl.textContent = '';
    if (updateStatusTextEl) updateStatusTextEl.textContent = 'Initiating update...';
    if (updateProgressContainer) updateProgressContainer.style.display = 'block';
    if (updateProgressBar) updateProgressBar.style.width = '0%';

    // Stop any previous polling (e.g., if it was polling for check result)
    if (otaPollingIntervalId) {
        clearInterval(otaPollingIntervalId);
        otaPollingIntervalId = null;
    }
    pollingForCheckResult = false; // Now polling for update progress

    fetch('/start-update', { method: 'POST' })
        .then(response => response.json())
        .then(data => {
            if (data.success) {
                console.log('Update process started successfully.');
                if (updateStatusTextEl) updateStatusTextEl.textContent = 'Update in progress...';
                // Start polling for status
                otaPollingIntervalId = setInterval(pollUpdateStatus, 2000); // Poll every 2 seconds
            } else {
                console.error('Failed to start update process:', data.message);
                if (updateErrorEl) updateErrorEl.textContent = `Error: ${data.message || 'Could not start update.'}`;
                if (updateStatusTextEl) updateStatusTextEl.textContent = 'Update failed to start.';
                if (installUpdateBtn) installUpdateBtn.disabled = false; // Re-enable if start failed
                if (checkUpdateBtn) checkUpdateBtn.disabled = false;
                if (updateProgressContainer) updateProgressContainer.style.display = 'none';
            }
        })
        .catch(error => {
            console.error('Error starting firmware update:', error);
            if (updateErrorEl) updateErrorEl.textContent = 'Communication error when trying to start the update.';
            if (updateStatusTextEl) updateStatusTextEl.textContent = 'Update start error.';
            if (installUpdateBtn) installUpdateBtn.disabled = false;
            if (checkUpdateBtn) checkUpdateBtn.disabled = false;
            if (updateProgressContainer) updateProgressContainer.style.display = 'none';
        });
}

function pollUpdateStatus() {
    const currentVersionEl = document.getElementById('current-version');
    const availableVersionEl = document.getElementById('available-version');
    const releaseNotesEl = document.getElementById('release-notes');
    const updateAvailableSection = document.getElementById('update-available-section');
    const updateStatusTextEl = document.getElementById('update-status-text');
    const updateErrorEl = document.getElementById('update-error-message');
    const updateProgressBar = document.getElementById('update-progress-bar');
    const updateProgressContainer = document.getElementById('update-progress-container');
    const installUpdateBtn = document.getElementById('install-update-btn');
    const checkUpdateBtn = document.getElementById('check-update-btn');

    fetch('/update-status')
        .then(response => response.json())
        .then(data => {
            console.log('[DBG] pollUpdateStatus - Raw data from /update-status:', JSON.stringify(data)); // Log raw data
            console.log('[DBG] pollUpdateStatus - pollingForCheckResult:', pollingForCheckResult);
            console.log('[DBG] pollUpdateStatus - data.status_code:', data.status_code, '(OTA_STATUS_STATE.CHECKING_VERSION is', OTA_STATUS_STATE.CHECKING_VERSION + ')');
            console.log('[DBG] pollUpdateStatus - data.is_update_available_info:', data.is_update_available_info);

            // Always update current version display from polled data if available
            if (currentVersionEl && data.current_firmware_version_info) {
                 currentVersionEl.textContent = data.current_firmware_version_info;
            }

            if (updateStatusTextEl) updateStatusTextEl.textContent = data.status_message || 'Fetching status...';
            if (updateErrorEl && data.error_message_info && data.error_message_info.length > 0) {
                // Prioritize error from UpdateInfo if present
                updateErrorEl.textContent = `Details: ${data.error_message_info}`;
            } else if (updateErrorEl && data.status_code >= OTA_STATUS_STATE.ERROR_WIFI) {
                // Otherwise, use status_message if it's an error state and no specific error_message_info
                updateErrorEl.textContent = data.status_message;
            } else if (updateErrorEl) {
                updateErrorEl.textContent = ''; // Clear previous errors if not an error state
            }

            if (data.status_code >= OTA_STATUS_STATE.DOWNLOADING && data.status_code <= OTA_STATUS_STATE.WRITING) {
                 if (updateProgressContainer) updateProgressContainer.style.display = 'block';
                 if (updateProgressBar) updateProgressBar.style.width = `${data.progress || 0}%`;
            } else if (data.status_code !== OTA_STATUS_STATE.SUCCESS) { // Don't hide progress bar immediately on success, wait for reboot message
                 // Hide progress for non-active download/write states unless it's success
                // if (updateProgressContainer) updateProgressContainer.style.display = 'none';
            }

            if (pollingForCheckResult) {
                console.log('[DBG] pollUpdateStatus - Inside pollingForCheckResult block');
                if (data.status_code === OTA_STATUS_STATE.CHECKING_VERSION && data.is_update_available_info) {
                    console.log('[DBG] pollUpdateStatus - Update IS available condition MET.');
                    console.log('Update available (polled):', data.available_firmware_version_info);
                    if (availableVersionEl) availableVersionEl.textContent = data.available_firmware_version_info;
                    if (releaseNotesEl) releaseNotesEl.textContent = data.release_notes_info || 'No release notes provided.';
                    if (updateAvailableSection) updateAvailableSection.style.display = 'block';
                    if (installUpdateBtn) installUpdateBtn.disabled = false;
                    if (updateStatusTextEl) updateStatusTextEl.textContent = 'Update available: ' + data.available_firmware_version_info;
                    
                    if (otaPollingIntervalId) clearInterval(otaPollingIntervalId);
                    otaPollingIntervalId = null;
                    pollingForCheckResult = false;
                    if (checkUpdateBtn) checkUpdateBtn.disabled = false; // Re-enable check button

                } else if (data.status_code === OTA_STATUS_STATE.IDLE || 
                           data.status_code >= OTA_STATUS_STATE.ERROR_WIFI) {
                    // This condition means the check process has definitively finished (either IDLE or an error state).
                    // If it's IDLE, we trust the is_update_available_info from the last check result (which is part of data).
                    console.log('[DBG] pollUpdateStatus - Check complete (IDLE or ERROR) condition MET.');
                    
                    if (data.status_code === OTA_STATUS_STATE.IDLE && data.is_update_available_info) {
                         // This case should ideally be caught by the first IF block, but as a safeguard:
                        console.log('[DBG] pollUpdateStatus - IDLE but update_available_info is true. Displaying update.');
                        if (availableVersionEl) availableVersionEl.textContent = data.available_firmware_version_info;
                        if (releaseNotesEl) releaseNotesEl.textContent = data.release_notes_info || 'No release notes provided.';
                        if (updateAvailableSection) updateAvailableSection.style.display = 'block';
                        if (installUpdateBtn) installUpdateBtn.disabled = false;
                        if (updateStatusTextEl) updateStatusTextEl.textContent = 'Update available: ' + data.available_firmware_version_info;
                    } else {
                        // No update found, or an error occurred during the check.
                        console.log('Check complete (polled): No update or error. Status message:', data.status_message);
                        if (availableVersionEl) availableVersionEl.textContent = 'N/A';
                        if (updateAvailableSection) updateAvailableSection.style.display = 'none';
                        if (installUpdateBtn) installUpdateBtn.disabled = true;

                        if (data.status_code === OTA_STATUS_STATE.IDLE && !data.is_update_available_info && !(data.error_message_info && data.error_message_info.length > 0) && !(data.status_message && data.status_message.toLowerCase().includes("error"))) {
                            updateStatusTextEl.textContent = data.status_message || 'Firmware is up to date.';
                        } else if (data.status_code >= OTA_STATUS_STATE.ERROR_WIFI && updateStatusTextEl) {
                            updateStatusTextEl.textContent = "Check failed."; // Error message is already in updateErrorEl
                        } else {
                            // Default for IDLE if no specific error message and no update
                            updateStatusTextEl.textContent = data.status_message || 'No update available.';
                        }
                    }

                    if (otaPollingIntervalId) clearInterval(otaPollingIntervalId);
                    otaPollingIntervalId = null;
                    pollingForCheckResult = false;
                    if (checkUpdateBtn) checkUpdateBtn.disabled = false; // Re-enable check button
                } else if (data.status_code === OTA_STATUS_STATE.CHECKING_VERSION) {
                    // Still checking, and data.is_update_available_info was false (or this block wouldn't be reached).
                    // Continue polling.
                    console.log('[DBG] pollUpdateStatus - Still CHECKING_VERSION, update not yet confirmed. Continuing poll.');
                    // updateStatusTextEl is already being updated with "Checking..." or similar.
                    // checkUpdateBtn remains disabled.
                } 
                // Implicitly, if none of the above, something unexpected happened or state is mid-transition.
                // The current logic will just continue polling, which is safe.

            } else { // Polling for update IN PROGRESS (not for check result)
                console.log('[DBG] pollUpdateStatus - Inside polling for UPDATE IN PROGRESS block.');
                if (data.status_code === OTA_STATUS_STATE.SUCCESS) {
                    console.log('[DBG] pollUpdateStatus - Update SUCCESS (during update progress) condition MET.');
                    console.log('Update successful (polled)!');
                    if (updateStatusTextEl) updateStatusTextEl.textContent = data.status_message || 'Update successful! Device will reboot.';
                    if (updateProgressBar) updateProgressBar.style.width = '100%';
                    if (updateProgressContainer) updateProgressContainer.style.display = 'block'; 
                    if (otaPollingIntervalId) clearInterval(otaPollingIntervalId);
                    otaPollingIntervalId = null;
                    // Buttons remain disabled as device reboots.
                    alert('Firmware update successful! The device will now reboot with the new version. This page may become unresponsive.');
                } else if (data.status_code >= OTA_STATUS_STATE.ERROR_WIFI) { // Any error state during update
                    console.log('[DBG] pollUpdateStatus - Update ERROR (during update progress) condition MET.');
                    console.error('Update failed (polled):', data.status_message);
                    // updateStatusTextEl and updateErrorEl handled by general logic above.
                    if (updateStatusTextEl) updateStatusTextEl.textContent = 'Update failed.';
                    if (otaPollingIntervalId) clearInterval(otaPollingIntervalId);
                    otaPollingIntervalId = null;
                    if (installUpdateBtn) installUpdateBtn.disabled = false; 
                    if (checkUpdateBtn) checkUpdateBtn.disabled = false;
                } else if (data.status_code === OTA_STATUS_STATE.IDLE && otaPollingIntervalId !== null) {
                    console.log('[DBG] pollUpdateStatus - Update process became IDLE UNEXPECTEDLY (during update progress) condition MET.');
                    // Update process became IDLE unexpectedly.
                    console.warn("OTA process became IDLE unexpectedly during update progress polling.");
                    if (updateStatusTextEl) updateStatusTextEl.textContent = 'Update process interrupted.';
                     if (!updateErrorEl.textContent) { 
                        updateErrorEl.textContent = "Update process finished unexpectedly.";
                    }
                    if (otaPollingIntervalId) clearInterval(otaPollingIntervalId);
                    otaPollingIntervalId = null;
                    if (installUpdateBtn) installUpdateBtn.disabled = false;
                    if (checkUpdateBtn) checkUpdateBtn.disabled = false;
                } else {
                    console.log('[DBG] pollUpdateStatus - Update IN PROGRESS (continuing poll for update progress).');
                }
            }
        })
        .catch(error => {
            console.error('Error polling update status:', error);
            if (updateErrorEl) updateErrorEl.textContent = 'Error fetching update status from device.';
            if (otaPollingIntervalId) clearInterval(otaPollingIntervalId);
            otaPollingIntervalId = null;
            pollingForCheckResult = false;
            // Re-enable buttons on communication error
            if (checkUpdateBtn) checkUpdateBtn.disabled = false;
            if (installUpdateBtn && !installUpdateBtn.disabled) { // Only if it was potentially active
                 // No, keep it disabled unless an update was actually available before poll error.
                 // Better to rely on next successful check to re-enable it.
            }
        });
}