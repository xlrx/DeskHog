function showScreen(screenId) {
    const screens = ['config-screen'];
    screens.forEach(id => {
        const el = document.getElementById(id);
        if (el) el.classList.add('hidden');
    });
    const screenToShow = document.getElementById(screenId);
    if (screenToShow) screenToShow.classList.remove('hidden');
    
    let title = "DeskHog Configuration";
    document.getElementById('page-title').textContent = title;
}

// Handle WiFi form submission
function saveWifiConfig() {
    const form = document.getElementById('wifi-form');
    const formData = new FormData(form);
    const globalActionStatusEl = document.getElementById('global-action-status');
    
    fetch('/api/actions/save-wifi', { 
        method: 'POST',
        body: formData
    })
    .then(response => response.json())
    .then(data => {
        if (data && data.status === 'queued') {
            console.log("Save WiFi action successfully queued.", data.message);
            if (globalActionStatusEl) {
                globalActionStatusEl.textContent = data.message || "Save WiFi initiated. Device will attempt to connect.";
                globalActionStatusEl.className = 'status-message info';
                globalActionStatusEl.style.display = 'block';
                setTimeout(() => {
                    if (globalActionStatusEl.textContent === (data.message || "Save WiFi initiated. Device will attempt to connect.")) {
                        globalActionStatusEl.style.display = 'none';
                        globalActionStatusEl.textContent = '';
                        globalActionStatusEl.className = 'status-message';
                    }
                }, 5000);
            }
        } else {
            const errorMessage = (data && data.message) ? data.message : "Failed to initiate WiFi save.";
            console.error("Failed to initiate WiFi save:", errorMessage);
            if (globalActionStatusEl) {
                globalActionStatusEl.textContent = errorMessage;
                globalActionStatusEl.className = 'status-message error';
                globalActionStatusEl.style.display = 'block';
                setTimeout(() => {
                    if (globalActionStatusEl.className.includes('error')) {
                        globalActionStatusEl.style.display = 'none';
                        globalActionStatusEl.textContent = '';
                        globalActionStatusEl.className = 'status-message';
                    }
                }, 7000);
            }
        }
    })
    .catch(() => {
        console.error("Communication error trying to save WiFi.");
        if (globalActionStatusEl) {
            globalActionStatusEl.textContent = "Communication error trying to save WiFi.";
            globalActionStatusEl.className = 'status-message error';
            globalActionStatusEl.style.display = 'block';
            setTimeout(() => {
                if (globalActionStatusEl.className.includes('error')) {
                    globalActionStatusEl.style.display = 'none';
                    globalActionStatusEl.textContent = '';
                    globalActionStatusEl.className = 'status-message';
                }
            }, 7000);
        }
    });
    
    return false; 
}

// Handle device config form submission
function saveDeviceConfig() {
    const form = document.getElementById('device-form');
    const formData = new FormData(form);
    const globalActionStatusEl = document.getElementById('global-action-status');
    
    fetch('/api/actions/save-device-config', { 
        method: 'POST',
        body: formData
    })
    .then(response => response.json())
    .then(data => {
        if (data && data.status === 'queued') {
            console.log("Save device config action successfully queued.", data.message);
            if (globalActionStatusEl) {
                globalActionStatusEl.textContent = data.message || "Device configuration save initiated.";
                globalActionStatusEl.className = 'status-message info';
                globalActionStatusEl.style.display = 'block';
                setTimeout(() => {
                     if (globalActionStatusEl.textContent === (data.message || "Device configuration save initiated.")) {
                        globalActionStatusEl.style.display = 'none';
                        globalActionStatusEl.textContent = '';
                        globalActionStatusEl.className = 'status-message';
                    }
                }, 5000);
            }
        } else {
            const errorMessage = (data && data.message) ? data.message : "Failed to initiate device config save.";
            console.error("Failed to initiate device config save:", errorMessage);
            if (globalActionStatusEl) {
                globalActionStatusEl.textContent = errorMessage;
                globalActionStatusEl.className = 'status-message error';
                globalActionStatusEl.style.display = 'block';
                setTimeout(() => {
                    if (globalActionStatusEl.className.includes('error')) {
                        globalActionStatusEl.style.display = 'none';
                        globalActionStatusEl.textContent = '';
                        globalActionStatusEl.className = 'status-message';
                    }
                }, 7000);
            }
        }
    })
    .catch(() => {
        console.error("Communication error saving device config.");
        if (globalActionStatusEl) {
            globalActionStatusEl.textContent = "Communication error saving device config.";
            globalActionStatusEl.className = 'status-message error';
            globalActionStatusEl.style.display = 'block';
            setTimeout(() => {
                if (globalActionStatusEl.className.includes('error')) {
                    globalActionStatusEl.style.display = 'none';
                    globalActionStatusEl.textContent = '';
                    globalActionStatusEl.className = 'status-message';
                }
            }, 7000);
        }
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
    const globalActionStatusEl = document.getElementById('global-action-status');

    fetch('/api/actions/save-insight', {
        method: 'POST',
        body: formData
    })
    .then(response => response.json())
    .then(data => {
        if (data && data.status === 'queued') {
            console.log("Add insight action successfully queued.", data.message);
            form.reset();
            
            if (globalActionStatusEl) {
                globalActionStatusEl.textContent = data.message || "Insight submission initiated. List will update shortly.";
                globalActionStatusEl.className = 'status-message info';
                globalActionStatusEl.style.display = 'block';
                setTimeout(() => {
                    if (globalActionStatusEl.textContent === (data.message || "Insight submission initiated. List will update shortly.")) {
                        globalActionStatusEl.style.display = 'none';
                        globalActionStatusEl.textContent = '';
                        globalActionStatusEl.className = 'status-message';
                    }
                }, 5000);
            }
        } else {
            const errorMessage = (data && data.message) ? data.message : "Failed to initiate save insight due to an unexpected server response.";
            console.error("Failed to initiate save insight:", errorMessage);
            if (globalActionStatusEl) {
                globalActionStatusEl.textContent = errorMessage;
                globalActionStatusEl.className = 'status-message error';
                globalActionStatusEl.style.display = 'block';
                setTimeout(() => {
                    if (globalActionStatusEl.className.includes('error')) {
                         globalActionStatusEl.style.display = 'none';
                         globalActionStatusEl.textContent = '';
                         globalActionStatusEl.className = 'status-message';
                    }
                }, 7000);
            }
        }
    })
    .catch((error) => {
        console.error("Communication error saving insight:", error);
        if (globalActionStatusEl) {
            globalActionStatusEl.textContent = "Communication error saving insight.";
            globalActionStatusEl.className = 'status-message error';
            globalActionStatusEl.style.display = 'block';
            setTimeout(() => {
                if (globalActionStatusEl.className.includes('error')) {
                    globalActionStatusEl.style.display = 'none';
                    globalActionStatusEl.textContent = '';
                    globalActionStatusEl.className = 'status-message';
                }
            }, 7000);
        }
    });
    
    return false;
}

// Delete insight
function deleteInsight(id) {
    if (!confirm('Are you sure you want to delete this insight?')) {
        return;
    }
    const globalActionStatusEl = document.getElementById('global-action-status');
    const formData = new FormData();
    formData.append('id', id);

    fetch('/api/actions/delete-insight', { 
        method: 'POST',
        body: formData
    })
    .then(response => response.json())
    .then(data => {
        if (data && data.status === 'queued') {
            console.log("Delete insight action successfully queued.", data.message);
            if (globalActionStatusEl) {
                globalActionStatusEl.textContent = data.message || "Delete insight initiated.";
                globalActionStatusEl.className = 'status-message info';
                globalActionStatusEl.style.display = 'block';
                setTimeout(() => {
                    if (globalActionStatusEl.textContent === (data.message || "Delete insight initiated.")) {
                        globalActionStatusEl.style.display = 'none';
                        globalActionStatusEl.textContent = '';
                        globalActionStatusEl.className = 'status-message';
                    }
                }, 5000);
            }
        } else {
            const errorMessage = (data && data.message) ? data.message : "Failed to initiate delete insight.";
            console.error("Failed to initiate delete insight:", errorMessage);
            if (globalActionStatusEl) {
                globalActionStatusEl.textContent = errorMessage;
                globalActionStatusEl.className = 'status-message error';
                globalActionStatusEl.style.display = 'block';
                setTimeout(() => {
                    if (globalActionStatusEl.className.includes('error')) {
                        globalActionStatusEl.style.display = 'none';
                        globalActionStatusEl.textContent = '';
                        globalActionStatusEl.className = 'status-message';
                    }
                }, 7000);
            }
        }
    })
    .catch(() => {
        console.error("Communication error deleting insight.");
        if (globalActionStatusEl) {
            globalActionStatusEl.textContent = "Communication error deleting insight.";
            globalActionStatusEl.className = 'status-message error';
            globalActionStatusEl.style.display = 'block';
            setTimeout(() => {
                if (globalActionStatusEl.className.includes('error')) {
                    globalActionStatusEl.style.display = 'none';
                    globalActionStatusEl.textContent = '';
                    globalActionStatusEl.className = 'status-message';
                }
            }, 7000);
        }
    });
}

// Load insights list - UI update part will be in pollApiStatus
function _updateInsightsListUI(insights) {
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
function _updateNetworksListUI(networks) {
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
    const globalActionStatusEl = document.getElementById('global-action-status');
    fetch('/api/actions/start-wifi-scan', { method: 'POST' })
        .then(response => {
            if (!response.ok) {
                return response.json().then(errData => {
                    throw new Error(errData.message || `HTTP error ${response.status}`);
                }).catch(() => {
                    throw new Error(`HTTP error ${response.status}`);
                });
            }
            return response.json();
        })
        .then(data => {
            if (data.status === 'queued') {
                console.log("WiFi scan action successfully queued:", data.message);
                const ssidSelect = document.getElementById('ssid');
                if (globalActionStatusEl) {
                    globalActionStatusEl.textContent = data.message || "WiFi scan requested. List will update shortly.";
                    globalActionStatusEl.className = 'status-message info';
                    globalActionStatusEl.style.display = 'block';
                    setTimeout(() => {
                        if (globalActionStatusEl.textContent === (data.message || "WiFi scan requested. List will update shortly.")) {
                            globalActionStatusEl.style.display = 'none';
                            globalActionStatusEl.textContent = '';
                            globalActionStatusEl.className = 'status-message';
                        }
                    }, 5000);
                }
                if (ssidSelect && (ssidSelect.options.length <= 1 || ssidSelect.firstChild.value === "")) {
                     ssidSelect.innerHTML = '<option>Scan requested, list updating...</option>';
                }

            } else {
                const errorMessage = `Failed to queue WiFi scan: ${data.message || 'Unexpected server response.'}`;
                console.error(errorMessage);
                if (globalActionStatusEl) {
                    globalActionStatusEl.textContent = errorMessage;
                    globalActionStatusEl.className = 'status-message error';
                    globalActionStatusEl.style.display = 'block';
                }
                document.getElementById('ssid').innerHTML = '<option>Scan request issue.</option>';
            }
        })
        .catch(error => {
            console.error('Error requesting network scan:', error.message);
            if (globalActionStatusEl) {
                globalActionStatusEl.textContent = `Error requesting scan: ${error.message}`;
                globalActionStatusEl.className = 'status-message error';
                globalActionStatusEl.style.display = 'block';
            }
            document.getElementById('ssid').innerHTML = '<option value="">Error starting scan</option>';
        });
}

// Main function to poll /api/status and update UI
let lastProcessedAction = null;
let lastProcessedActionMessage = "";
let initialDeviceConfigLoaded = false;

function pollApiStatus() {
    fetch('/api/status')
        .then(response => response.json())
        .then(data => {
            const portalStatus = data.portal;
            const globalActionStatusEl = document.getElementById('global-action-status');

            if (portalStatus && globalActionStatusEl) {
                if (portalStatus.action_in_progress && portalStatus.action_in_progress !== 'NONE') {
                    globalActionStatusEl.textContent = `Processing: ${portalStatus.action_in_progress.replace(/_/g, ' ').toLowerCase()}...`;
                    globalActionStatusEl.className = 'status-message info'; 
                    globalActionStatusEl.style.display = 'block';
                    lastProcessedAction = null; 
                }
                else if (portalStatus.last_action_completed && portalStatus.last_action_completed !== 'NONE') {
                    const completedActionKey = portalStatus.last_action_completed + '-' + (portalStatus.last_action_status || 'UNKNOWN') + '-' + (portalStatus.last_action_message || 'NO_MSG');

                    if (lastProcessedAction !== completedActionKey) {
                        console.log(`Action completed: ${portalStatus.last_action_completed}, Status: ${portalStatus.last_action_status}, Msg: ${portalStatus.last_action_message}`);

                        if (portalStatus.last_action_status === 'SUCCESS') {
                            let successMsg = portalStatus.last_action_message || `${portalStatus.last_action_completed.replace(/_/g, ' ')} successful.`;
                            if (!portalStatus.last_action_message) {
                                switch (portalStatus.last_action_completed) {
                                    case 'SAVE_WIFI': 
                                        successMsg = 'WiFi configuration saved. Device will attempt to connect.'; 
                                        if (data.wifi && data.wifi.is_connected) {
                                            successMsg += ` Connected to ${data.wifi.connected_ssid}.`;
                                        } else if (data.wifi) {
                                            successMsg += ' Checking connection...';
                                        }
                                        break;
                                    case 'SAVE_DEVICE_CONFIG': successMsg = 'Device configuration saved.'; break;
                                    case 'SAVE_INSIGHT': successMsg = 'New insight saved.'; break;
                                    case 'DELETE_INSIGHT': successMsg = 'Insight deleted.'; break;
                                }
                            }
                            globalActionStatusEl.textContent = successMsg;
                            globalActionStatusEl.className = 'status-message success';
                            globalActionStatusEl.style.display = 'block';

                            setTimeout(() => {
                                if (globalActionStatusEl.textContent === successMsg && !globalActionStatusEl.className.includes('info')) { 
                                    globalActionStatusEl.style.display = 'none';
                                    globalActionStatusEl.textContent = '';
                                    globalActionStatusEl.className = 'status-message';
                                }
                            }, 7000); 

                        } else if (portalStatus.last_action_status === 'ERROR') {
                            const errorMsgText = portalStatus.last_action_message || "An unknown error occurred with " + portalStatus.last_action_completed + ".";
                            console.error("Action failed:", errorMsgText);
                            if (globalActionStatusEl) {
                                globalActionStatusEl.textContent = `Status: ${errorMsgText}`;
                                globalActionStatusEl.className = 'status-message error';
                                globalActionStatusEl.style.display = 'block';
                                setTimeout(() => {
                                    if (globalActionStatusEl.className.includes('error')) {
                                        globalActionStatusEl.style.display = 'none';
                                        globalActionStatusEl.textContent = '';
                                        globalActionStatusEl.className = 'status-message';
                                    }
                                }, 10000); 
                            }
                        }
                        lastProcessedAction = completedActionKey;
                    }
                } else {
                    if (globalActionStatusEl.className.includes('info') && globalActionStatusEl.textContent.startsWith('Processing:')) {
                    } 
                }
            } else if (globalActionStatusEl && portalStatus === null) { 
                 if (globalActionStatusEl.style.display !== 'none' && !globalActionStatusEl.className.includes('success') && !globalActionStatusEl.className.includes('error')){
                    globalActionStatusEl.style.display = 'none';
                    globalActionStatusEl.textContent = '';
                    globalActionStatusEl.className = 'status-message';
                 }
            }

            // 2. Update WiFi Info
            if (data.wifi) {
                _updateNetworksListUI(data.wifi.networks);
                const wifiStatusEl = document.getElementById('wifi-connection-status');
                if (wifiStatusEl) {
                    if (data.wifi.is_connected) {
                        wifiStatusEl.textContent = `Connected to ${data.wifi.connected_ssid} (${data.wifi.ip_address})`;
                    } else if (portalStatus && portalStatus.action_in_progress === 'SAVE_WIFI') {
                        wifiStatusEl.textContent = "Attempting to connect...";
                    } else {
                        wifiStatusEl.textContent = "Not Connected";
                    }
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
            const globalActionStatusEl = document.getElementById('global-action-status');
            if(globalActionStatusEl) {
                globalActionStatusEl.textContent = 'Lost connection to device. Please check and refresh.';
                globalActionStatusEl.className = 'status-message error';
                globalActionStatusEl.style.display = 'block';
            }
        });
}

// Load current configuration - UI update part will be in pollApiStatus
function _updateDeviceConfigUI(config) {
    if (!initialDeviceConfigLoaded) {
        if (config.team_id !== undefined) {
            document.getElementById('teamId').value = config.team_id;
        }
        if (config.api_key_display !== undefined) { 
            document.getElementById('apiKey').value = config.api_key_display;
        }
        initialDeviceConfigLoaded = true;
    }
}

// Initialize page
document.addEventListener('DOMContentLoaded', function() {
    const hash = window.location.hash.substr(1);
    if (hash && ['config-screen'].includes(hash)) {
        showScreen(hash);
    } else {
        showScreen('config-screen');
    }

    const checkUpdateBtn = document.getElementById('check-update-btn');
    const installUpdateBtn = document.getElementById('install-update-btn');

    if (checkUpdateBtn) {
        checkUpdateBtn.addEventListener('click', requestCheckFirmwareUpdate);
    }
    if (installUpdateBtn) {
        installUpdateBtn.addEventListener('click', requestStartFirmwareUpdate);
    }

    pollApiStatus();
    setInterval(pollApiStatus, 5000);

    const refreshBtn = document.getElementById('refresh-networks-btn');
    if(refreshBtn) {
        refreshBtn.addEventListener('click', requestScanNetworks);
    }
});

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

    fetch('/api/actions/check-ota-update', { method: 'POST' })
        .then(response => response.json())
        .then(data => {
            if (data.status === 'initiated') {
                console.log('Firmware check initiated.');
            } else {
                console.error('Failed to initiate firmware check:', data.message);
                if(checkUpdateBtn) checkUpdateBtn.disabled = false;
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
    const availableVersionContainerEl = document.getElementById('available-version-container');
    const releaseNotesEl = document.getElementById('release-notes');
    const updateAvailableSection = document.getElementById('update-available-section');
    const installUpdateBtn = document.getElementById('install-update-btn');
    const updateStatusTextEl = document.getElementById('update-status-text');
    const updateErrorEl = document.getElementById('update-error-message');
    const checkUpdateBtn = document.getElementById('check-update-btn');
    const updateProgressContainer = document.getElementById('update-progress-container');
    const updateProgressBar = document.getElementById('update-progress-bar');

    let portalOtaMessageDisplayed = false;
    if (portalData && portalData.portal_ota_action_message) {
        if (updateStatusTextEl) updateStatusTextEl.textContent = portalData.portal_ota_action_message;
        portalOtaMessageDisplayed = true;
        if (portalData.action_in_progress === 'CHECK_OTA_UPDATE' || portalData.action_in_progress === 'START_OTA_UPDATE') {
            if (checkUpdateBtn) checkUpdateBtn.disabled = true;
            if (installUpdateBtn) installUpdateBtn.disabled = true;
        } else {
        }
    }

    if (currentVersionEl) currentVersionEl.textContent = otaData.current_firmware_version || 'N/A';
    
    if (availableVersionContainerEl && availableVersionEl) {
        if (otaData.update_available && otaData.available_version && otaData.available_version !== 'N/A') {
            availableVersionEl.textContent = otaData.available_version;
            availableVersionContainerEl.style.display = '';
        } else {
            availableVersionEl.textContent = 'N/A';
            availableVersionContainerEl.style.display = 'none';
        }
    }

    if (releaseNotesEl) releaseNotesEl.textContent = otaData.update_available ? (otaData.release_notes || 'No release notes.') : '';

    if (updateAvailableSection) updateAvailableSection.style.display = otaData.update_available ? 'block' : 'none';
    
    if (installUpdateBtn) {
        installUpdateBtn.disabled = !otaData.update_available || 
                                  (portalData && (portalData.action_in_progress === 'START_OTA_UPDATE' || portalData.action_in_progress === 'CHECK_OTA_UPDATE')) ||
                                  otaData.status_code === OTA_STATUS_STATE.DOWNLOADING || 
                                  otaData.status_code === OTA_STATUS_STATE.WRITING;
    }
    
    if (checkUpdateBtn) {
        checkUpdateBtn.disabled = (portalData && (portalData.action_in_progress === 'CHECK_OTA_UPDATE' || portalData.action_in_progress === 'START_OTA_UPDATE')) ||
                                otaData.status_code === OTA_STATUS_STATE.CHECKING_VERSION || 
                                otaData.status_code === OTA_STATUS_STATE.DOWNLOADING || 
                                otaData.status_code === OTA_STATUS_STATE.WRITING;
    }

    if (!portalOtaMessageDisplayed || (portalData && portalData.portal_ota_action_message && portalData.portal_ota_action_message.includes("Successfully dispatched"))) {
        if (updateStatusTextEl) {
            let displayMessage = otaData.status_message || 'Idle';
            if (portalOtaMessageDisplayed && otaData.status_message && otaData.status_message !== "Idle") {
                updateStatusTextEl.textContent = displayMessage;
            } else if (!portalOtaMessageDisplayed) {
                 updateStatusTextEl.textContent = displayMessage;
            }
        }
    }
    
    if (updateErrorEl) {
        updateErrorEl.textContent = otaData.error_message || '';
    }

    if (otaData.status_code === OTA_STATUS_STATE.DOWNLOADING || otaData.status_code === OTA_STATUS_STATE.WRITING) {
        if (updateProgressContainer) updateProgressContainer.style.display = 'block';
        if (updateProgressBar) updateProgressBar.style.width = `${otaData.progress || 0}%`;
    } else {
        if (otaData.status_code !== OTA_STATUS_STATE.SUCCESS && updateProgressContainer) {
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