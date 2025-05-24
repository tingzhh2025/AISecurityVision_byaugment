// ONVIF Device Discovery JavaScript

class ONVIFDiscovery {
    constructor() {
        this.apiBase = '/api';
        this.discoveredDevices = [];
        this.isScanning = false;
        this.currentDevice = null;

        this.initializeElements();
        this.bindEvents();
        this.updateSystemStatus();
    }

    initializeElements() {
        // Control elements
        this.scanBtn = document.getElementById('scan-btn');
        this.scanTimeout = document.getElementById('scan-timeout');
        this.refreshBtn = document.getElementById('refresh-btn');

        // Status elements
        this.systemStatus = document.getElementById('system-status');
        this.systemStatusText = document.getElementById('system-status-text');
        this.discoveryStatus = document.getElementById('discovery-status');
        this.deviceCount = document.getElementById('device-count');

        // Progress elements
        this.discoveryProgress = document.getElementById('discovery-progress');
        this.progressFill = document.getElementById('progress-fill');
        this.progressText = document.getElementById('progress-text');

        // Results elements
        this.devicesContainer = document.getElementById('devices-container');
        this.noDevices = document.getElementById('no-devices');

        // Modal elements
        this.configModal = document.getElementById('config-modal');
        this.modalClose = document.getElementById('modal-close');
        this.modalDeviceInfo = document.getElementById('modal-device-info');
        this.configForm = document.getElementById('config-form');
        this.deviceUsername = document.getElementById('device-username');
        this.devicePassword = document.getElementById('device-password');
        this.autoStart = document.getElementById('auto-start');
        this.configureBtn = document.getElementById('configure-btn');
        this.cancelBtn = document.getElementById('cancel-btn');
        this.testConnectionBtn = document.getElementById('test-connection-btn');

        // Loading overlay
        this.loadingOverlay = document.getElementById('loading-overlay');
    }

    bindEvents() {
        // Scan button
        this.scanBtn.addEventListener('click', () => this.startDiscovery());

        // Refresh button
        this.refreshBtn.addEventListener('click', () => this.startDiscovery());

        // Modal events
        this.modalClose.addEventListener('click', () => this.closeModal());
        this.cancelBtn.addEventListener('click', () => this.closeModal());
        this.configureBtn.addEventListener('click', () => this.configureDevice());
        this.testConnectionBtn.addEventListener('click', () => this.testConnection());

        // Close modal on outside click
        this.configModal.addEventListener('click', (e) => {
            if (e.target === this.configModal) {
                this.closeModal();
            }
        });

        // Form submission
        this.configForm.addEventListener('submit', (e) => {
            e.preventDefault();
            this.configureDevice();
        });
    }

    async updateSystemStatus() {
        try {
            const response = await fetch(`${this.apiBase}/system/status`);
            const data = await response.json();

            if (data.status === 'running') {
                this.systemStatus.className = 'status-indicator online';
                this.systemStatusText.textContent = 'System Online';
            } else {
                this.systemStatus.className = 'status-indicator warning';
                this.systemStatusText.textContent = 'System Warning';
            }
        } catch (error) {
            this.systemStatus.className = 'status-indicator offline';
            this.systemStatusText.textContent = 'System Offline';
        }
    }

    async startDiscovery() {
        if (this.isScanning) return;

        this.isScanning = true;
        this.updateScanButton(true);
        this.showProgress();

        const timeout = parseInt(this.scanTimeout.value);
        this.discoveryStatus.textContent = 'Scanning network...';

        try {
            // Start progress animation
            this.animateProgress(timeout);

            const response = await fetch(`${this.apiBase}/source/discover`);
            const data = await response.json();

            if (data.status === 'success') {
                this.discoveredDevices = data.devices || [];
                this.displayResults();
                this.discoveryStatus.textContent = `Found ${this.discoveredDevices.length} devices`;
            } else {
                throw new Error(data.message || 'Discovery failed');
            }
        } catch (error) {
            console.error('Discovery error:', error);
            this.discoveryStatus.textContent = 'Discovery failed';
            this.showError('Failed to discover devices: ' + error.message);
        } finally {
            this.isScanning = false;
            this.updateScanButton(false);
            this.hideProgress();
        }
    }

    updateScanButton(scanning) {
        if (scanning) {
            this.scanBtn.disabled = true;
            this.scanBtn.classList.add('scanning');
            this.scanBtn.querySelector('.btn-text').textContent = 'Scanning...';
            this.scanBtn.querySelector('.btn-icon').textContent = '⏳';
        } else {
            this.scanBtn.disabled = false;
            this.scanBtn.classList.remove('scanning');
            this.scanBtn.querySelector('.btn-text').textContent = 'Scan for ONVIF Devices';
            this.scanBtn.querySelector('.btn-icon').textContent = '🔍';
        }
    }

    showProgress() {
        this.discoveryProgress.style.display = 'block';
        this.progressFill.style.width = '0%';
    }

    hideProgress() {
        setTimeout(() => {
            this.discoveryProgress.style.display = 'none';
        }, 500);
    }

    animateProgress(duration) {
        const startTime = Date.now();
        const animate = () => {
            const elapsed = Date.now() - startTime;
            const progress = Math.min((elapsed / duration) * 100, 100);

            this.progressFill.style.width = progress + '%';

            if (progress < 100 && this.isScanning) {
                requestAnimationFrame(animate);
            }
        };
        animate();
    }

    displayResults() {
        this.deviceCount.textContent = `${this.discoveredDevices.length} devices found`;

        if (this.discoveredDevices.length === 0) {
            this.devicesContainer.innerHTML = '<div class="no-devices" id="no-devices"><div class="no-devices-icon">📷</div><div class="no-devices-text">No ONVIF devices found</div><div class="no-devices-hint">Make sure devices are on the same network</div></div>';
            this.refreshBtn.style.display = 'none';
            return;
        }

        this.refreshBtn.style.display = 'inline-block';

        const devicesGrid = document.createElement('div');
        devicesGrid.className = 'devices-grid';

        this.discoveredDevices.forEach((device, index) => {
            const deviceCard = this.createDeviceCard(device, index);
            devicesGrid.appendChild(deviceCard);
        });

        this.devicesContainer.innerHTML = '';
        this.devicesContainer.appendChild(devicesGrid);
    }

    createDeviceCard(device, index) {
        const card = document.createElement('div');
        card.className = 'device-card';

        const isConfigured = device.configured || false;

        card.innerHTML = `
            <div class="device-header">
                <div>
                    <div class="device-name">${device.name || 'ONVIF Camera'}</div>
                    <div class="device-ip">${device.ip_address || device.ipAddress}</div>
                </div>
                <div class="device-status ${isConfigured ? 'status-configured' : 'status-discovered'}">
                    ${isConfigured ? 'Configured' : 'Discovered'}
                </div>
            </div>

            <div class="device-info">
                <div class="info-row">
                    <span class="info-label">Manufacturer:</span>
                    <span class="info-value">${device.manufacturer || 'Unknown'}</span>
                </div>
                <div class="info-row">
                    <span class="info-label">Model:</span>
                    <span class="info-value">${device.model || 'Unknown'}</span>
                </div>
                <div class="info-row">
                    <span class="info-label">Stream URI:</span>
                    <span class="info-value">${device.stream_uri || device.streamUri || 'Not available'}</span>
                </div>
                <div class="info-row">
                    <span class="info-label">Authentication:</span>
                    <span class="info-value">${device.requires_auth || device.requiresAuth ? 'Required' : 'None'}</span>
                </div>
            </div>

            <div class="device-actions">
                <button class="btn-configure" onclick="onvifDiscovery.openConfigModal(${index})" ${isConfigured ? 'disabled' : ''}>
                    ${isConfigured ? 'Already Configured' : 'Configure'}
                </button>
            </div>
        `;

        return card;
    }

    openConfigModal(deviceIndex) {
        this.currentDevice = this.discoveredDevices[deviceIndex];

        // Populate device info
        this.modalDeviceInfo.innerHTML = `
            <div class="info-row">
                <span class="info-label">Device:</span>
                <span class="info-value">${this.currentDevice.name || 'ONVIF Camera'}</span>
            </div>
            <div class="info-row">
                <span class="info-label">IP Address:</span>
                <span class="info-value">${this.currentDevice.ip_address || this.currentDevice.ipAddress}</span>
            </div>
            <div class="info-row">
                <span class="info-label">Stream URI:</span>
                <span class="info-value">${this.currentDevice.stream_uri || this.currentDevice.streamUri || 'Not available'}</span>
            </div>
        `;

        // Reset form
        this.deviceUsername.value = 'admin';
        this.devicePassword.value = '';
        this.autoStart.checked = true;

        // Show modal
        this.configModal.style.display = 'flex';
    }

    closeModal() {
        this.configModal.style.display = 'none';
        this.currentDevice = null;
    }

    async testConnection() {
        if (!this.currentDevice) return;

        const username = this.deviceUsername.value.trim();
        const password = this.devicePassword.value;

        if (!username) {
            this.showError('Please enter a username to test the connection');
            return;
        }

        // Disable the test button and show loading state
        this.testConnectionBtn.disabled = true;
        this.testConnectionBtn.textContent = 'Testing...';

        try {
            const deviceId = this.currentDevice.uuid || this.currentDevice.device_id ||
                           `onvif_${this.currentDevice.ip_address || this.currentDevice.ipAddress}_${this.currentDevice.port || 80}`;

            // Create a test request (we'll use the same endpoint but with a test flag)
            const response = await fetch(`${this.apiBase}/source/add-discovered`, {
                method: 'POST',
                headers: {
                    'Content-Type': 'application/json'
                },
                body: JSON.stringify({
                    device_id: deviceId,
                    username: username,
                    password: password,
                    test_only: true  // Flag to indicate this is just a test
                })
            });

            const data = await response.json();

            if (response.ok) {
                this.showSuccess('✅ Connection test successful! Credentials are valid.');
            } else if (response.status === 401) {
                this.showError('❌ Authentication failed: Invalid username or password');
            } else {
                throw new Error(data.message || 'Connection test failed');
            }
        } catch (error) {
            console.error('Connection test error:', error);
            this.showError('❌ Connection test failed: ' + error.message);
        } finally {
            // Re-enable the test button
            this.testConnectionBtn.disabled = false;
            this.testConnectionBtn.textContent = 'Test Connection';
        }
    }

    async configureDevice() {
        if (!this.currentDevice) return;

        const username = this.deviceUsername.value.trim();
        const password = this.devicePassword.value;

        this.showLoading();

        try {
            const deviceId = this.currentDevice.uuid || this.currentDevice.device_id ||
                           `onvif_${this.currentDevice.ip_address || this.currentDevice.ipAddress}_${this.currentDevice.port || 80}`;

            const response = await fetch(`${this.apiBase}/source/add-discovered`, {
                method: 'POST',
                headers: {
                    'Content-Type': 'application/json'
                },
                body: JSON.stringify({
                    device_id: deviceId,
                    username: username,
                    password: password
                })
            });

            const data = await response.json();

            if (response.ok) {
                this.showSuccess('Device configured successfully!');
                this.closeModal();
                // Refresh the device list
                setTimeout(() => this.startDiscovery(), 1000);
            } else {
                throw new Error(data.message || 'Configuration failed');
            }
        } catch (error) {
            console.error('Configuration error:', error);
            this.showError('Failed to configure device: ' + error.message);
        } finally {
            this.hideLoading();
        }
    }

    showLoading() {
        this.loadingOverlay.classList.remove('hidden');
    }

    hideLoading() {
        this.loadingOverlay.classList.add('hidden');
    }

    showSuccess(message) {
        // Simple success notification
        alert('✅ ' + message);
    }

    showError(message) {
        // Simple error notification
        alert('❌ ' + message);
    }
}

// Initialize the ONVIF Discovery interface
let onvifDiscovery;

document.addEventListener('DOMContentLoaded', () => {
    onvifDiscovery = new ONVIFDiscovery();
});
