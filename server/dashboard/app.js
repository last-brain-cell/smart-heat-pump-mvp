/**
 * Heat Pump Monitor Dashboard
 * ===========================
 * Supports both v1 (single-phase) and v2 (3-phase) devices.
 */

// Configuration
const API_BASE = '/api';
const WS_URL = `ws://${window.location.host}/ws`;
const REFRESH_INTERVAL = 2000;

// State
let currentDevice = null;
let ws = null;
let reconnectAttempts = 0;
let restConnected = false;
let wsConnected = false;

// DOM Elements
const elements = {
    connectionStatus: document.getElementById('connection-status'),
    lastUpdate: document.getElementById('last-update'),
    statDevices: document.getElementById('stat-devices'),
    statReadings: document.getElementById('stat-readings'),
    deviceList: document.getElementById('device-list'),
    currentDevice: document.getElementById('current-device'),
    // Temperature
    tempInlet: document.getElementById('temp-inlet'),
    tempOutlet: document.getElementById('temp-outlet'),
    tempAmbient: document.getElementById('temp-ambient'),
    tempCompressor: document.getElementById('temp-compressor'),
    // Phase 1
    p1Voltage: document.getElementById('p1-voltage'),
    p1Current: document.getElementById('p1-current'),
    p1Power: document.getElementById('p1-power'),
    p1Pf: document.getElementById('p1-pf'),
    p1Freq: document.getElementById('p1-freq'),
    p1Energy: document.getElementById('p1-energy'),
    // Phase 2
    p2Voltage: document.getElementById('p2-voltage'),
    p2Current: document.getElementById('p2-current'),
    p2Power: document.getElementById('p2-power'),
    p2Pf: document.getElementById('p2-pf'),
    p2Freq: document.getElementById('p2-freq'),
    p2Energy: document.getElementById('p2-energy'),
    // Phase 3
    p3Voltage: document.getElementById('p3-voltage'),
    p3Current: document.getElementById('p3-current'),
    p3Power: document.getElementById('p3-power'),
    p3Pf: document.getElementById('p3-pf'),
    p3Freq: document.getElementById('p3-freq'),
    p3Energy: document.getElementById('p3-energy'),
    // Averages
    avgVoltage: document.getElementById('avg-voltage'),
    totalCurrent: document.getElementById('total-current'),
    totalPower: document.getElementById('total-power'),
    avgPf: document.getElementById('avg-pf'),
    avgFreq: document.getElementById('avg-freq'),
    totalEnergy: document.getElementById('total-energy'),
    // Mode indicator
    electricalMode: document.getElementById('electrical-mode'),
    // Pressure (v1 only)
    pressureHigh: document.getElementById('pressure-high'),
    pressureLow: document.getElementById('pressure-low')
};

// ===========================================
// API Functions
// ===========================================

async function fetchAPI(endpoint) {
    try {
        const response = await fetch(`${API_BASE}${endpoint}`);
        if (!response.ok) throw new Error(`HTTP ${response.status}`);
        const data = await response.json();
        restConnected = true;
        setConnectionStatus(true);
        return data;
    } catch (error) {
        console.error(`API Error (${endpoint}):`, error);
        restConnected = false;
        if (!wsConnected) {
            setConnectionStatus(false);
        }
        return null;
    }
}

async function fetchStats() {
    const stats = await fetchAPI('/stats');
    if (stats) {
        elements.statDevices.textContent = stats.devices_online;
        elements.statReadings.textContent = formatNumber(stats.total_readings);
    }
}

async function fetchDevices() {
    const devices = await fetchAPI('/devices');
    if (devices && devices.length > 0) {
        // Auto-select first device if none selected
        if (!currentDevice) {
            currentDevice = devices[0].device_id;
            elements.currentDevice.textContent = currentDevice;
        }
        renderDevices(devices);
    }
}

async function fetchDeviceStatus(deviceId) {
    if (!deviceId) return;
    const status = await fetchAPI(`/devices/${deviceId}/status`);
    if (status) {
        updateReadings(status);
    }
}

async function fetchLatestReading(deviceId) {
    const reading = await fetchAPI(`/devices/${deviceId}/readings/latest`);
    if (reading) {
        updateReadings(reading);
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
            wsConnected = true;
            setConnectionStatus(true);
            reconnectAttempts = 0;
        };

        ws.onclose = () => {
            console.log('WebSocket disconnected');
            wsConnected = false;
            if (!restConnected) {
                setConnectionStatus(false);
            }
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
}

// ===========================================
// Version Detection
// ===========================================

function isV2Data(status) {
    return status.phase1_voltage !== null && status.phase1_voltage !== undefined;
}

// ===========================================
// Phase Rendering
// ===========================================

function setPhaseEmpty(prefix) {
    elements[prefix + 'Voltage'].textContent = '--';
    elements[prefix + 'Current'].textContent = '--';
    elements[prefix + 'Power'].textContent = '--';
    elements[prefix + 'Pf'].textContent = '--';
    elements[prefix + 'Freq'].textContent = '--';
    elements[prefix + 'Energy'].textContent = '--';
}

function updatePhaseFromV1(status) {
    elements.electricalMode.textContent = '1-Phase';

    // Phase 1 = v1 single-phase data
    setValueWithAlert(elements.p1Voltage, status.voltage, 'V', null, null, 210, 250);
    setValueWithAlert(elements.p1Current, status.current, 'A', 12, 15);
    setValueWithAlert(elements.p1Power, status.power, 'W');
    elements.p1Pf.textContent = '--';
    elements.p1Freq.textContent = '--';
    elements.p1Energy.textContent = '--';

    // Phase 2 & 3 empty
    setPhaseEmpty('p2');
    setPhaseEmpty('p3');

    // Averages = single phase values
    setValueWithAlert(elements.avgVoltage, status.voltage, 'V');
    setValueWithAlert(elements.totalCurrent, status.current, 'A');
    setValueWithAlert(elements.totalPower, status.power, 'W');
    elements.avgPf.textContent = '--';
    elements.avgFreq.textContent = '--';
    elements.totalEnergy.textContent = '--';

    // Show v1-only sections
    document.querySelectorAll('.v1-only').forEach(el => el.style.display = '');
}

function updatePhaseFromV2(status) {
    elements.electricalMode.textContent = '3-Phase';

    const phases = [
        { prefix: 'p1', v: 'phase1' },
        { prefix: 'p2', v: 'phase2' },
        { prefix: 'p3', v: 'phase3' }
    ];

    let voltSum = 0, voltCount = 0;
    let currSum = 0, powerSum = 0, pfSum = 0, pfCount = 0;
    let freqSum = 0, freqCount = 0, energySum = 0;

    phases.forEach(({ prefix, v }) => {
        const voltage = status[v + '_voltage'];
        const current = status[v + '_current'];
        const power = status[v + '_power'];
        const pf = status[v + '_pf'];
        const freq = status[v + '_frequency'];
        const energy = status[v + '_energy'];

        setValueWithAlert(elements[prefix + 'Voltage'], voltage, 'V', null, null, 210, 250);
        setValueWithAlert(elements[prefix + 'Current'], current, 'A', 12, 15);
        setValueWithAlert(elements[prefix + 'Power'], power, 'W');
        elements[prefix + 'Pf'].textContent = pf != null ? pf.toFixed(2) : '--';
        elements[prefix + 'Freq'].textContent = freq != null ? freq.toFixed(1) : '--';
        elements[prefix + 'Energy'].textContent = energy != null ? energy.toFixed(2) : '--';

        if (voltage != null) { voltSum += voltage; voltCount++; }
        if (current != null) { currSum += current; }
        if (power != null) { powerSum += power; }
        if (pf != null) { pfSum += pf; pfCount++; }
        if (freq != null) { freqSum += freq; freqCount++; }
        if (energy != null) { energySum += energy; }
    });

    // Compute averages/totals
    elements.avgVoltage.textContent = voltCount > 0 ? (voltSum / voltCount).toFixed(1) : '--';
    elements.totalCurrent.textContent = currSum > 0 ? currSum.toFixed(2) : '--';
    elements.totalPower.textContent = powerSum > 0 ? powerSum.toFixed(1) : '--';
    elements.avgPf.textContent = pfCount > 0 ? (pfSum / pfCount).toFixed(2) : '--';
    elements.avgFreq.textContent = freqCount > 0 ? (freqSum / freqCount).toFixed(1) : '--';
    elements.totalEnergy.textContent = energySum > 0 ? energySum.toFixed(2) : '--';

    // Hide v1-only sections
    document.querySelectorAll('.v1-only').forEach(el => el.style.display = 'none');
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

function updateReadings(status) {
    if (!status) return;

    // Temperature (always shown)
    setValueWithAlert(elements.tempInlet, status.temp_inlet, 'C');
    setValueWithAlert(elements.tempOutlet, status.temp_outlet, 'C');
    setValueWithAlert(elements.tempAmbient, status.temp_ambient, 'C');
    setValueWithAlert(elements.tempCompressor, status.temp_compressor, 'C', 85, 95);

    // Electrical: detect v1 vs v2
    if (isV2Data(status)) {
        updatePhaseFromV2(status);
    } else {
        updatePhaseFromV1(status);
    }

    // Pressure (v1 only, visibility handled by v1-only class)
    setValueWithAlert(elements.pressureHigh, status.pressure_high, 'PSI', 400, 450);
    setValueWithAlert(elements.pressureLow, status.pressure_low, 'PSI', null, null, 20, 40);

    updateLastUpdate();
}

function updateReadingsFromMQTT(data) {
    if (!data) return;

    const temps = data.temperature || {};
    setValueWithAlert(elements.tempInlet, temps.inlet, 'C');
    setValueWithAlert(elements.tempOutlet, temps.outlet, 'C');
    setValueWithAlert(elements.tempAmbient, temps.ambient, 'C');
    setValueWithAlert(elements.tempCompressor, temps.compressor, 'C', 85, 95);

    const elec = data.electrical || {};

    if (elec.phase1) {
        // V2 MQTT format: 3-phase
        elements.electricalMode.textContent = '3-Phase';

        const phases = [
            { prefix: 'p1', data: elec.phase1 },
            { prefix: 'p2', data: elec.phase2 || {} },
            { prefix: 'p3', data: elec.phase3 || {} }
        ];

        let voltSum = 0, voltCount = 0, currSum = 0, powerSum = 0;
        let pfSum = 0, pfCount = 0, freqSum = 0, freqCount = 0, energySum = 0;

        phases.forEach(({ prefix, data: ph }) => {
            setValueWithAlert(elements[prefix + 'Voltage'], ph.voltage, 'V', null, null, 210, 250);
            setValueWithAlert(elements[prefix + 'Current'], ph.current, 'A', 12, 15);
            setValueWithAlert(elements[prefix + 'Power'], ph.power, 'W');
            elements[prefix + 'Pf'].textContent = ph.power_factor != null ? ph.power_factor.toFixed(2) : '--';
            elements[prefix + 'Freq'].textContent = ph.frequency != null ? ph.frequency.toFixed(1) : '--';
            elements[prefix + 'Energy'].textContent = ph.energy != null ? ph.energy.toFixed(2) : '--';

            if (ph.voltage != null) { voltSum += ph.voltage; voltCount++; }
            if (ph.current != null) currSum += ph.current;
            if (ph.power != null) powerSum += ph.power;
            if (ph.power_factor != null) { pfSum += ph.power_factor; pfCount++; }
            if (ph.frequency != null) { freqSum += ph.frequency; freqCount++; }
            if (ph.energy != null) energySum += ph.energy;
        });

        elements.avgVoltage.textContent = voltCount > 0 ? (voltSum / voltCount).toFixed(1) : '--';
        elements.totalCurrent.textContent = currSum > 0 ? currSum.toFixed(2) : '--';
        elements.totalPower.textContent = powerSum > 0 ? powerSum.toFixed(1) : '--';
        elements.avgPf.textContent = pfCount > 0 ? (pfSum / pfCount).toFixed(2) : '--';
        elements.avgFreq.textContent = freqCount > 0 ? (freqSum / freqCount).toFixed(1) : '--';
        elements.totalEnergy.textContent = energySum > 0 ? energySum.toFixed(2) : '--';

        document.querySelectorAll('.v1-only').forEach(el => el.style.display = 'none');
    } else {
        // V1 MQTT format: single-phase
        elements.electricalMode.textContent = '1-Phase';
        setValueWithAlert(elements.p1Voltage, elec.voltage, 'V', null, null, 210, 250);
        setValueWithAlert(elements.p1Current, elec.current, 'A', 12, 15);
        setValueWithAlert(elements.p1Power, elec.power, 'W');
        elements.p1Pf.textContent = '--';
        elements.p1Freq.textContent = '--';
        elements.p1Energy.textContent = '--';
        setPhaseEmpty('p2');
        setPhaseEmpty('p3');

        elements.avgVoltage.textContent = elec.voltage != null ? formatValue(elec.voltage) : '--';
        elements.totalCurrent.textContent = elec.current != null ? formatValue(elec.current) : '--';
        elements.totalPower.textContent = elec.power != null ? formatValue(elec.power) : '--';
        elements.avgPf.textContent = '--';
        elements.avgFreq.textContent = '--';
        elements.totalEnergy.textContent = '--';

        document.querySelectorAll('.v1-only').forEach(el => el.style.display = '');
    }

    const pressure = data.pressure || {};
    setValueWithAlert(elements.pressureHigh, pressure.high, 'PSI', 400, 450);
    setValueWithAlert(elements.pressureLow, pressure.low, 'PSI', null, null, 20, 40);
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

    let alertClass = '';
    if (critHigh && value >= critHigh) alertClass = 'critical';
    else if (warnHigh && value >= warnHigh) alertClass = 'warning';
    else if (critLow && value <= critLow) alertClass = 'critical';
    else if (warnLow && value <= warnLow) alertClass = 'warning';

    element.className = `value ${alertClass}`;
}

function setConnectionStatus(connected) {
    if (connected) {
        elements.connectionStatus.textContent = 'Connected';
        elements.connectionStatus.className = 'conn-badge online';
    } else {
        elements.connectionStatus.textContent = 'Disconnected';
        elements.connectionStatus.className = 'conn-badge offline';
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

function selectDevice(deviceId) {
    currentDevice = deviceId;
    elements.currentDevice.textContent = deviceId;

    document.querySelectorAll('.device-card').forEach(card => {
        card.classList.remove('selected');
    });
    event.currentTarget.classList.add('selected');

    fetchDeviceStatus(deviceId);
}

// ===========================================
// Initialization
// ===========================================

async function init() {
    console.log('Initializing dashboard...');

    // Fetch devices first to auto-select, then get status
    await fetchStats();
    await fetchDevices();
    if (currentDevice) {
        await fetchDeviceStatus(currentDevice);
    }

    connectWebSocket();

    setInterval(() => {
        fetchStats();
        fetchDeviceStatus(currentDevice);
    }, REFRESH_INTERVAL);
}

document.addEventListener('DOMContentLoaded', init);
