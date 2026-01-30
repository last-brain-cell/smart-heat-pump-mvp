/**
 * Heat Pump Monitor Dashboard
 * ===========================
 */

// Configuration
const API_BASE = '/api';
const WS_URL = `ws://${window.location.host}/ws`;
const REFRESH_INTERVAL = 10000; // 10 seconds

// State
let currentDevice = 'site1';
let ws = null;
let reconnectAttempts = 0;

// DOM Elements
const elements = {
    connectionStatus: document.getElementById('connection-status'),
    lastUpdate: document.getElementById('last-update'),
    statDevices: document.getElementById('stat-devices'),
    statReadings: document.getElementById('stat-readings'),
    statAlerts: document.getElementById('stat-alerts'),
    deviceList: document.getElementById('device-list'),
    currentDevice: document.getElementById('current-device'),
    alertsList: document.getElementById('alerts-list'),
    // Sensor readings
    tempInlet: document.getElementById('temp-inlet'),
    tempOutlet: document.getElementById('temp-outlet'),
    tempAmbient: document.getElementById('temp-ambient'),
    tempCompressor: document.getElementById('temp-compressor'),
    voltage: document.getElementById('voltage'),
    current: document.getElementById('current'),
    power: document.getElementById('power'),
    pressureHigh: document.getElementById('pressure-high'),
    pressureLow: document.getElementById('pressure-low'),
    statusCompressor: document.getElementById('status-compressor'),
    statusFan: document.getElementById('status-fan'),
    statusDefrost: document.getElementById('status-defrost')
};

// ===========================================
// API Functions
// ===========================================

async function fetchAPI(endpoint) {
    try {
        const response = await fetch(`${API_BASE}${endpoint}`);
        if (!response.ok) throw new Error(`HTTP ${response.status}`);
        return await response.json();
    } catch (error) {
        console.error(`API Error (${endpoint}):`, error);
        return null;
    }
}

async function fetchStats() {
    const stats = await fetchAPI('/stats');
    if (stats) {
        elements.statDevices.textContent = stats.devices_online;
        elements.statReadings.textContent = formatNumber(stats.total_readings);
        elements.statAlerts.textContent = stats.active_alerts;
    }
}

async function fetchDevices() {
    const devices = await fetchAPI('/devices');
    if (devices) {
        renderDevices(devices);
    }
}

async function fetchDeviceStatus(deviceId) {
    const status = await fetchAPI(`/devices/${deviceId}/status`);
    if (status) {
        updateReadings(status);
    }
}

async function fetchLatestReading(deviceId) {
    const reading = await fetchAPI(`/devices/${deviceId}/readings/latest`);
    if (reading) {
        updateReadingsFromData(reading);
    }
}

async function fetchAlerts() {
    const alerts = await fetchAPI('/alerts?unacknowledged_only=true&limit=10');
    if (alerts) {
        renderAlerts(alerts);
    }
}

async function acknowledgeAlert(alertId) {
    try {
        const response = await fetch(`${API_BASE}/alerts/${alertId}/acknowledge`, {
            method: 'POST'
        });
        if (response.ok) {
            fetchAlerts();
            fetchStats();
        }
    } catch (error) {
        console.error('Error acknowledging alert:', error);
    }
}

// ===========================================
// WebSocket
// ===========================================

function connectWebSocket() {
    try {
        ws = new WebSocket(WS_URL);

        ws.onopen = () => {
            console.log('WebSocket connected');
            setConnectionStatus(true);
            reconnectAttempts = 0;
        };

        ws.onclose = () => {
            console.log('WebSocket disconnected');
            setConnectionStatus(false);
            // Reconnect with exponential backoff
            const delay = Math.min(1000 * Math.pow(2, reconnectAttempts), 30000);
            reconnectAttempts++;
            setTimeout(connectWebSocket, delay);
        };

        ws.onerror = (error) => {
            console.error('WebSocket error:', error);
        };

        ws.onmessage = (event) => {
            try {
                const message = JSON.parse(event.data);
                handleWebSocketMessage(message);
            } catch (error) {
                console.error('Error parsing WebSocket message:', error);
            }
        };
    } catch (error) {
        console.error('WebSocket connection error:', error);
        setTimeout(connectWebSocket, 5000);
    }
}

function handleWebSocketMessage(message) {
    console.log('WebSocket message:', message);

    if (message.type === 'sensor_data' && message.device_id === currentDevice) {
        updateReadingsFromMQTT(message.data);
        updateLastUpdate();
    }

    if (message.type === 'alert') {
        fetchAlerts();
        fetchStats();
    }
}

// ===========================================
// Rendering Functions
// ===========================================

function renderDevices(devices) {
    elements.deviceList.innerHTML = devices.map(device => `
        <div class="device-card ${device.is_online ? '' : 'offline'} ${device.device_id === currentDevice ? 'selected' : ''}"
             onclick="selectDevice('${device.device_id}')">
            <div class="device-header">
                <span class="device-name">${device.name || device.device_id}</span>
                <span class="device-status ${device.is_online ? 'online' : 'offline'}">
                    ${device.is_online ? 'Online' : 'Offline'}
                </span>
            </div>
            <div class="device-info">
                ${device.location || 'Location not set'}
            </div>
            <div class="device-info">
                Last seen: ${device.last_seen ? formatTime(device.last_seen) : 'Never'}
            </div>
        </div>
    `).join('');
}

function renderAlerts(alerts) {
    if (alerts.length === 0) {
        elements.alertsList.innerHTML = '<p class="no-alerts">No active alerts</p>';
        return;
    }

    elements.alertsList.innerHTML = alerts.map(alert => `
        <div class="alert-item">
            <div class="alert-info">
                <div class="alert-icon ${alert.alert_level}">
                    ${alert.alert_level === 'critical' ? '!' : '⚠'}
                </div>
                <div>
                    <div class="alert-type">${formatAlertType(alert.alert_type)}</div>
                    <div class="alert-message">${alert.message || `Value: ${alert.value}`}</div>
                </div>
            </div>
            <div class="alert-time">${formatTime(alert.timestamp)}</div>
            <button class="alert-action" onclick="acknowledgeAlert(${alert.id})">
                Acknowledge
            </button>
        </div>
    `).join('');
}

function updateReadings(status) {
    if (!status) return;

    setValueWithAlert(elements.tempInlet, status.temp_inlet, '°C');
    setValueWithAlert(elements.tempOutlet, status.temp_outlet, '°C');
    setValueWithAlert(elements.tempAmbient, status.temp_ambient, '°C');
    setValueWithAlert(elements.tempCompressor, status.temp_compressor, '°C', 85, 95);

    setValueWithAlert(elements.voltage, status.voltage, 'V', null, null, 210, 250);
    setValueWithAlert(elements.current, status.current, 'A', 12, 15);
    setValueWithAlert(elements.power, status.power, 'W');

    setValueWithAlert(elements.pressureHigh, status.pressure_high, 'PSI', 400, 450);
    setValueWithAlert(elements.pressureLow, status.pressure_low, 'PSI', null, null, 20, 40);

    setStatusBadge(elements.statusCompressor, status.compressor_running);
    setStatusBadge(elements.statusFan, status.fan_running);
    setStatusBadge(elements.statusDefrost, status.defrost_active);

    updateLastUpdate();
}

function updateReadingsFromData(data) {
    if (!data) return;

    setValueWithAlert(elements.tempInlet, data.temp_inlet, '°C');
    setValueWithAlert(elements.tempOutlet, data.temp_outlet, '°C');
    setValueWithAlert(elements.tempAmbient, data.temp_ambient, '°C');
    setValueWithAlert(elements.tempCompressor, data.temp_compressor, '°C', 85, 95);

    setValueWithAlert(elements.voltage, data.voltage, 'V', null, null, 210, 250);
    setValueWithAlert(elements.current, data.current, 'A', 12, 15);
    setValueWithAlert(elements.power, data.power, 'W');

    setValueWithAlert(elements.pressureHigh, data.pressure_high, 'PSI', 400, 450);
    setValueWithAlert(elements.pressureLow, data.pressure_low, 'PSI', null, null, 20, 40);

    setStatusBadge(elements.statusCompressor, data.compressor_running);
    setStatusBadge(elements.statusFan, data.fan_running);
    setStatusBadge(elements.statusDefrost, data.defrost_active);

    updateLastUpdate();
}

function updateReadingsFromMQTT(data) {
    if (!data) return;

    const temps = data.temperature || {};
    const elec = data.electrical || {};
    const pressure = data.pressure || {};
    const status = data.status || {};

    setValueWithAlert(elements.tempInlet, temps.inlet, '°C');
    setValueWithAlert(elements.tempOutlet, temps.outlet, '°C');
    setValueWithAlert(elements.tempAmbient, temps.ambient, '°C');
    setValueWithAlert(elements.tempCompressor, temps.compressor, '°C', 85, 95);

    setValueWithAlert(elements.voltage, elec.voltage, 'V', null, null, 210, 250);
    setValueWithAlert(elements.current, elec.current, 'A', 12, 15);
    setValueWithAlert(elements.power, elec.power, 'W');

    setValueWithAlert(elements.pressureHigh, pressure.high, 'PSI', 400, 450);
    setValueWithAlert(elements.pressureLow, pressure.low, 'PSI', null, null, 20, 40);

    setStatusBadge(elements.statusCompressor, status.compressor);
    setStatusBadge(elements.statusFan, status.fan);
    setStatusBadge(elements.statusDefrost, status.defrost);
}

// ===========================================
// Helper Functions
// ===========================================

function setValueWithAlert(element, value, unit, warnHigh, critHigh, critLow, warnLow) {
    if (value === null || value === undefined) {
        element.textContent = '--';
        element.className = 'value';
        return;
    }

    element.textContent = formatValue(value);

    // Check thresholds
    let alertClass = '';
    if (critHigh && value >= critHigh) alertClass = 'critical';
    else if (warnHigh && value >= warnHigh) alertClass = 'warning';
    else if (critLow && value <= critLow) alertClass = 'critical';
    else if (warnLow && value <= warnLow) alertClass = 'warning';

    element.className = `value ${alertClass}`;
}

function setStatusBadge(element, isOn) {
    if (isOn) {
        element.textContent = 'ON';
        element.className = 'status-badge on';
    } else {
        element.textContent = 'OFF';
        element.className = 'status-badge off';
    }
}

function setConnectionStatus(connected) {
    if (connected) {
        elements.connectionStatus.textContent = 'Connected';
        elements.connectionStatus.className = 'status-indicator online';
    } else {
        elements.connectionStatus.textContent = 'Disconnected';
        elements.connectionStatus.className = 'status-indicator offline';
    }
}

function updateLastUpdate() {
    elements.lastUpdate.textContent = `Last update: ${formatTime(new Date())}`;
}

function formatValue(value) {
    if (typeof value === 'number') {
        return value % 1 === 0 ? value.toString() : value.toFixed(1);
    }
    return value;
}

function formatNumber(num) {
    if (num >= 1000000) return (num / 1000000).toFixed(1) + 'M';
    if (num >= 1000) return (num / 1000).toFixed(1) + 'K';
    return num.toString();
}

function formatTime(timestamp) {
    const date = new Date(timestamp);
    const now = new Date();
    const diff = now - date;

    if (diff < 60000) return 'Just now';
    if (diff < 3600000) return `${Math.floor(diff / 60000)}m ago`;
    if (diff < 86400000) return `${Math.floor(diff / 3600000)}h ago`;

    return date.toLocaleString();
}

function formatAlertType(type) {
    return type.split('_').map(word =>
        word.charAt(0).toUpperCase() + word.slice(1).toLowerCase()
    ).join(' ');
}

function selectDevice(deviceId) {
    currentDevice = deviceId;
    elements.currentDevice.textContent = deviceId;

    // Update device card selection
    document.querySelectorAll('.device-card').forEach(card => {
        card.classList.remove('selected');
    });
    event.currentTarget.classList.add('selected');

    // Fetch new device data
    fetchDeviceStatus(deviceId);
}

// ===========================================
// Initialization
// ===========================================

async function init() {
    console.log('Initializing dashboard...');

    // Initial data fetch
    await Promise.all([
        fetchStats(),
        fetchDevices(),
        fetchDeviceStatus(currentDevice),
        fetchAlerts()
    ]);

    // Connect WebSocket for real-time updates
    connectWebSocket();

    // Set up periodic refresh
    setInterval(() => {
        fetchStats();
        fetchDeviceStatus(currentDevice);
        fetchAlerts();
    }, REFRESH_INTERVAL);
}

// Start the app
document.addEventListener('DOMContentLoaded', init);
