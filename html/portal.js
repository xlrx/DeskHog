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

    if(updateStatusTextEl) updateStatusTextEl.textContent = 'Checking for updates...';
    if(updateErrorEl) updateErrorEl.textContent = '';
    if(updateAvailableSection) updateAvailableSection.style.display = 'none';
    if(installUpdateBtn) installUpdateBtn.disabled = true;
    if(checkUpdateBtn) checkUpdateBtn.disabled = true;

    fetch('/check-update')
        .then(response => response.json())
        .then(data => {
            if (currentVersionEl) currentVersionEl.textContent = data.currentVersion || 'N/A';
            if (data.error && data.error.length > 0) {
                console.error('Error checking for update:', data.error);
                if(updateErrorEl) updateErrorEl.textContent = `Error: ${data.error}`;
                if(updateStatusTextEl) updateStatusTextEl.textContent = 'Check failed.';
                return;
            }

            if (data.updateAvailable) {
                console.log('Update available:', data.availableVersion);
                if (availableVersionEl) availableVersionEl.textContent = data.availableVersion;
                if (releaseNotesEl) releaseNotesEl.textContent = data.releaseNotes || 'No release notes provided.';
                if (updateAvailableSection) updateAvailableSection.style.display = 'block';
                if (installUpdateBtn) installUpdateBtn.disabled = false;
                if(updateStatusTextEl) updateStatusTextEl.textContent = 'Update available!';
            } else {
                console.log('No update available or already up-to-date.');
                if (availableVersionEl) availableVersionEl.textContent = 'N/A (Up to date)';
                if(updateStatusTextEl) updateStatusTextEl.textContent = 'Your firmware is up to date.';
            }
        })
        .catch(error => {
            console.error('Failed to fetch update status:', error);
            if (currentVersionEl) currentVersionEl.textContent = 'Error';
            if (availableVersionEl) availableVersionEl.textContent = 'Error';
            if(updateErrorEl) updateErrorEl.textContent = 'Failed to communicate with device to check for updates.';
            if(updateStatusTextEl) updateStatusTextEl.textContent = 'Error checking updates.';
        })
        .finally(() => {
            if(checkUpdateBtn) checkUpdateBtn.disabled = false;
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

    // Stop any previous polling
    if (otaPollingIntervalId) {
        clearInterval(otaPollingIntervalId);
        otaPollingIntervalId = null;
    }

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
    const updateStatusTextEl = document.getElementById('update-status-text');
    const updateErrorEl = document.getElementById('update-error-message');
    const updateProgressBar = document.getElementById('update-progress-bar');
    const updateProgressContainer = document.getElementById('update-progress-container');
    const installUpdateBtn = document.getElementById('install-update-btn');
    const checkUpdateBtn = document.getElementById('check-update-btn');

    fetch('/update-status')
        .then(response => response.json())
        .then(data => {
            console.log('OTA Status:', data);
            if (updateStatusTextEl) updateStatusTextEl.textContent = data.message || 'Fetching status...';
            if (updateProgressBar) updateProgressBar.style.width = `${data.progress || 0}%`;
            if (updateProgressContainer && updateProgressContainer.style.display === 'none' && data.progress > 0 && data.progress < 100) {
                updateProgressContainer.style.display = 'block';
            }

            // Check if the update is complete or has errored
            if (data.status === OTA_STATUS_STATE.SUCCESS) {
                console.log('Update successful!');
                if (updateStatusTextEl) updateStatusTextEl.textContent = data.message || 'Update successful! Device will reboot.';
                if (updateProgressBar) updateProgressBar.style.width = '100%';
                if (otaPollingIntervalId) clearInterval(otaPollingIntervalId);
                // Don't re-enable buttons, device is rebooting.
                alert('Firmware update successful! The device will now reboot with the new version. This page may become unresponsive.');
            } else if (data.status >= OTA_STATUS_STATE.ERROR_WIFI) { // Any error state
                console.error('Update failed:', data.message);
                if (updateErrorEl) updateErrorEl.textContent = `Error: ${data.message || 'An unknown error occurred.'}`;
                if (updateStatusTextEl) updateStatusTextEl.textContent = 'Update failed.';
                if (otaPollingIntervalId) clearInterval(otaPollingIntervalId);
                if (installUpdateBtn) installUpdateBtn.disabled = false; // Re-enable buttons on failure
                if (checkUpdateBtn) checkUpdateBtn.disabled = false;
            } else if (data.status === OTA_STATUS_STATE.IDLE && otaPollingIntervalId !== null && data.message.includes("No update available")){
                // This case could happen if a check completed while polling was active from a previous attempt
                // and cleared the "update available" state. Or if an update was aborted and status reset.
                // For now, just stop polling if we hit IDLE unexpectedly during an update.
                console.warn("OTA process became IDLE unexpectedly during update polling.");
                if (otaPollingIntervalId) clearInterval(otaPollingIntervalId);
                if (installUpdateBtn) installUpdateBtn.disabled = false;
                if (checkUpdateBtn) checkUpdateBtn.disabled = false;
                 if (!updateErrorEl.textContent) { // don't overwrite a more specific error
                    updateErrorEl.textContent = "Update process was interrupted or finished unexpectedly.";
                }
            }
        })
        .catch(error => {
            console.error('Error polling update status:', error);
            if (updateErrorEl) updateErrorEl.textContent = 'Error fetching update status from device.';
            // Potentially stop polling on comms error to avoid spamming
            if (otaPollingIntervalId) clearInterval(otaPollingIntervalId);
            if (installUpdateBtn) installUpdateBtn.disabled = false;
            if (checkUpdateBtn) checkUpdateBtn.disabled = false;
        });
}