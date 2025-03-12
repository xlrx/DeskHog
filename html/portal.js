function showScreen(screenId) {
    const screens = ['home-screen', 'config-screen', 'success-screen', 'error-screen'];
    screens.forEach(id => {
        document.getElementById(id).classList.add('hidden');
    });
    document.getElementById(screenId).classList.remove('hidden');
    
    // Update page title
    let title = "ESP32 WiFi Setup";
    if (screenId === 'config-screen') title = "WiFi Configuration";
    else if (screenId === 'success-screen') title = "WiFi Configuration Saved";
    else if (screenId === 'error-screen') title = "Configuration Error";
    document.getElementById('page-title').textContent = title;
    
    // Start countdown if success screen
    if (screenId === 'success-screen') {
        startCountdown();
        document.getElementById('progress-bar').style.width = '100%';
    }
}

// Handle form submission
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

// Refresh network list
function refreshNetworks() {
    fetch('/scan-networks')
    .then(response => response.text())
    .then(html => {
        document.getElementById('ssid').innerHTML = html;
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

// Initialize page
document.addEventListener('DOMContentLoaded', function() {
    // Check if we need to show a specific screen based on URL hash
    const hash = window.location.hash.substr(1);
    if (hash && ['home-screen', 'config-screen', 'success-screen', 'error-screen'].includes(hash)) {
        showScreen(hash);
    }
});