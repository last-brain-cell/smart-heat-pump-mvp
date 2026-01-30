# Stage 1: Heat Pump Monitoring System

## Table of Contents
1. [System Overview](#1-system-overview)
2. [Architecture](#2-architecture)
3. [Hardware Requirements](#3-hardware-requirements)
4. [Firmware](#4-firmware)
5. [Backend API](#5-backend-api)
6. [Data Model](#6-data-model)
7. [Observability & Monitoring](#7-observability--monitoring)
8. [Deployment Guide](#8-deployment-guide)
9. [Troubleshooting](#9-troubleshooting)

---

## 1. System Overview

### 1.1 Purpose

The Smart Heat Pump Monitoring System is a remote monitoring solution for heat pumps installed in locations without reliable WiFi. It uses cellular connectivity (GSM/GPRS) to transmit sensor data to a cloud backend for storage, visualization, and alerting.

### 1.2 Key Features

| Feature | Description |
|---------|-------------|
| **Remote Monitoring** | Real-time sensor data from any location with cellular coverage |
| **Cellular Connectivity** | GSM/GPRS via SIM800C - no WiFi required |
| **Multi-Sensor Support** | Temperature, voltage, current, power, pressure |
| **SMS Alerts** | Critical condition notifications to admin phone |
| **SMS Commands** | Remote status check and device restart |
| **Offline Buffering** | Stores 100 readings when network unavailable |
| **Real-time Dashboard** | WebSocket-based live updates |
| **Historical Data** | Time-series storage in InfluxDB |

### 1.3 Current Capabilities (Stage 1)

**What it does:**
- Reads sensor data every 10 seconds
- Buffers data locally in circular buffer (100 readings)
- Publishes to MQTT every 5 minutes
- Sends SMS alerts for critical conditions
- Responds to SMS commands (STATUS, RESET)
- Provides REST API for dashboard

**What it doesn't do (Stage 2):**
- Control heat pump operation
- Change settings remotely
- Scheduled operations

---

## 2. Architecture

### 2.1 System Diagram

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                           STAGE 1 ARCHITECTURE                              │
└─────────────────────────────────────────────────────────────────────────────┘

                                  FIELD INSTALLATION
┌──────────────────────────────────────────────────────────────────────────────┐
│                                                                              │
│  ┌──────────────┐     ┌──────────────┐     ┌──────────────────────────────┐ │
│  │  Heat Pump   │     │   Sensors    │     │        ESP32 + SIM800C       │ │
│  │    Unit      │────►│  (Temp/V/I)  │────►│      Monitoring Device       │ │
│  └──────────────┘     └──────────────┘     └──────────────┬───────────────┘ │
│                                                           │                  │
└───────────────────────────────────────────────────────────┼──────────────────┘
                                                            │
                                                            │ GPRS (Cellular)
                                                            │ MQTT Protocol
                                                            ▼
                                  CLOUD INFRASTRUCTURE
┌──────────────────────────────────────────────────────────────────────────────┐
│                                                                              │
│  ┌──────────────┐     ┌──────────────┐     ┌──────────────────────────────┐ │
│  │ MQTT Broker  │────►│   Backend    │────►│         InfluxDB             │ │
│  │  (Mosquitto) │     │  (FastAPI)   │     │    (Time-Series DB)          │ │
│  └──────────────┘     └──────┬───────┘     └──────────────────────────────┘ │
│                              │                                               │
│                              │ WebSocket                                     │
│                              ▼                                               │
│                       ┌──────────────┐                                       │
│                       │  Dashboard   │                                       │
│                       │   (Web UI)   │                                       │
│                       └──────────────┘                                       │
│                                                                              │
└──────────────────────────────────────────────────────────────────────────────┘

                                   SMS CHANNEL
┌──────────────────────────────────────────────────────────────────────────────┐
│                                                                              │
│  ┌──────────────┐                              ┌──────────────────────────┐  │
│  │    Admin     │◄────── SMS Alerts ──────────│        ESP32             │  │
│  │    Phone     │─────── SMS Commands ───────►│      (SIM800C)           │  │
│  └──────────────┘                              └──────────────────────────┘  │
│                                                                              │
└──────────────────────────────────────────────────────────────────────────────┘
```

### 2.2 Data Flow

```
1. SENSOR READING (every 10 seconds)
   ┌─────────┐    ┌─────────┐    ┌─────────┐    ┌─────────┐
   │ Sensors │───►│  ESP32  │───►│ Buffer  │───►│  Check  │
   │  (ADC)  │    │  Read   │    │  Store  │    │ Alerts  │
   └─────────┘    └─────────┘    └─────────┘    └────┬────┘
                                                     │
                                                     ▼ If Critical
                                               ┌─────────┐
                                               │Send SMS │
                                               │  Alert  │
                                               └─────────┘

2. MQTT PUBLISH (every 5 minutes)
   ┌─────────┐    ┌─────────┐    ┌─────────┐    ┌─────────┐
   │ Buffer  │───►│ Connect │───►│ Publish │───►│  Clear  │
   │  Data   │    │  GPRS   │    │  MQTT   │    │ Buffer  │
   └─────────┘    └─────────┘    └─────────┘    └─────────┘

3. BACKEND PROCESSING
   ┌─────────┐    ┌─────────┐    ┌─────────┐    ┌─────────┐
   │  MQTT   │───►│ Parse   │───►│  Store  │───►│Broadcast│
   │ Message │    │  JSON   │    │InfluxDB│    │WebSocket│
   └─────────┘    └─────────┘    └─────────┘    └─────────┘
```

### 2.3 Communication Protocols

| Channel | Protocol | Direction | Purpose |
|---------|----------|-----------|---------|
| Device → Server | MQTT/TLS | Publish | Sensor data, alerts |
| Server → Device | MQTT/TLS | Subscribe | Commands (Stage 2) |
| Device ↔ Admin | SMS | Bidirectional | Alerts, commands |
| Server → Dashboard | WebSocket | Push | Real-time updates |
| Dashboard → Server | REST API | Request | Historical data, status |

---

## 3. Hardware Requirements

### 3.1 Bill of Materials

| Component | Model/Specification | Qty | Est. Price (INR) |
|-----------|---------------------|-----|------------------|
| **Microcontroller** | ESP32 WROOM DevKit | 1 | ₹450 |
| **GSM Module** | SIM800C with antenna | 1 | ₹350 |
| **Temperature Sensors** | 10K NTC Thermistor | 4 | ₹120 |
| **Voltage Sensor** | ZMPT101B AC Voltage | 1 | ₹180 |
| **Current Sensor** | ACS712-20A | 1 | ₹120 |
| **Pressure Transducer** | 0-500 PSI, 0.5-4.5V (optional) | 2 | ₹1,200 |
| **Resistors** | 10K ohm 1/4W | 4 | ₹10 |
| **Capacitors** | 100nF ceramic | 6 | ₹30 |
| **Power Supply** | 5V 2A USB adapter | 1 | ₹150 |
| **SIM Card** | Data-enabled SIM | 1 | ₹100 |
| **Enclosure** | IP65 junction box | 1 | ₹300 |
| **Misc** | Wires, connectors, PCB | - | ₹200 |
| **TOTAL** | | | **₹3,210** |

### 3.2 Pin Connections

#### ESP32 GPIO Allocation

| GPIO | Function | Type | Notes |
|------|----------|------|-------|
| 16 | GSM RX | Serial | SIM800C TX → ESP32 RX |
| 17 | GSM TX | Serial | ESP32 TX → SIM800C RX |
| 34 | Temp Inlet | ADC1_CH6 | NTC voltage divider |
| 35 | Temp Outlet | ADC1_CH7 | NTC voltage divider |
| 32 | Temp Ambient | ADC1_CH4 | NTC voltage divider |
| 33 | Temp Compressor | ADC1_CH5 | NTC voltage divider |
| 36 (VP) | Voltage Sensor | ADC1_CH0 | ZMPT101B output |
| 39 (VN) | Current Sensor | ADC1_CH3 | ACS712 output |
| 25 | Pressure High | ADC2_CH8 | 0.5-4.5V signal |
| 26 | Pressure Low | ADC2_CH9 | 0.5-4.5V signal |
| 2 | Status LED | Digital | Built-in LED |

#### Wiring Diagram

```
                              ESP32 WROOM DevKit
                           ┌───────────────────────┐
                           │                       │
              3.3V ────────┤ 3V3             VIN  ├──────── 5V
                           │                       │
     NTC Inlet ────────────┤ D34            GND  ├──────── GND
    NTC Outlet ────────────┤ D35                  │
   NTC Ambient ────────────┤ D32             D2  ├──────── Status LED
NTC Compressor ────────────┤ D33                  │
                           │                       │
  ZMPT101B Out ────────────┤ VP (D36)        D16 ├──────── SIM800C TX
   ACS712 Out ─────────────┤ VN (D39)        D17 ├──────── SIM800C RX
                           │                       │
 Pressure High ────────────┤ D25                  │
  Pressure Low ────────────┤ D26                  │
                           │                       │
                           └───────────────────────┘


           NTC Thermistor Voltage Divider (x4)
           ───────────────────────────────────
                      3.3V
                       │
                       ├──── 10K Fixed Resistor
                       │
                       ├──────────────────────► ADC Pin (GPIO 34/35/32/33)
                       │
                       ├──── NTC Thermistor (10K @ 25°C)
                       │
                      GND


           SIM800C GSM Module
           ──────────────────
           ┌─────────────────┐
           │   SIM800C       │
           │                 │
    5V ────┤ VCC        TX  ├──────► ESP32 GPIO16 (RX)
   GND ────┤ GND        RX  ├◄────── ESP32 GPIO17 (TX)
           │                 │
           │    [Antenna]    │
           └─────────────────┘
```

### 3.3 Sensor Specifications

#### Temperature Sensors (NTC 10K)

| Parameter | Value |
|-----------|-------|
| Type | NTC Thermistor |
| Resistance @ 25°C | 10K ohm |
| Beta coefficient | 3950 |
| Range | -40°C to +125°C |
| Accuracy | ±1°C (with calibration) |

#### Voltage Sensor (ZMPT101B)

| Parameter | Value |
|-----------|-------|
| Input Voltage | 0-250V AC |
| Output Voltage | 0-3.3V (adjustable) |
| Accuracy | ±1% |
| Isolation | Transformer-coupled |

#### Current Sensor (ACS712-20A)

| Parameter | Value |
|-----------|-------|
| Range | ±20A |
| Sensitivity | 100mV/A |
| Zero-current output | 2.5V (VCC/2) |
| Accuracy | ±1.5% |

#### Pressure Transducer (Optional)

| Parameter | Value |
|-----------|-------|
| Range | 0-500 PSI |
| Output | 0.5V - 4.5V |
| Supply | 5V DC |
| Accuracy | ±0.5% FS |

---

## 4. Firmware

### 4.1 Module Structure

```
firmware/
├── firmware.ino          # Main entry point, setup() and loop()
├── config.h              # Configuration constants
└── src/
    ├── types.h           # Data structures and enums
    ├── globals.h         # Global variable declarations
    ├── sensors.h/.cpp    # Sensor reading functions
    ├── gsm.h/.cpp        # GSM/GPRS/SMS functions
    ├── mqtt.h/.cpp       # MQTT client functions
    ├── alerts.h/.cpp     # Alert threshold checking
    └── buffer.h/.cpp     # Circular data buffer
```

### 4.2 Main Loop Tasks

| Task | Interval | Description |
|------|----------|-------------|
| Sensor Read | 10 seconds | Read all sensors, check alerts, buffer data |
| SMS Check | 5 seconds | Check for incoming SMS commands |
| MQTT Publish | 5 minutes | Connect GPRS, publish buffered data |
| Watchdog Feed | Continuous | Reset hardware watchdog timer |
| Heartbeat LED | 5 seconds | Blink status LED |

### 4.3 Configuration (`config.h`)

```cpp
// Device Identity
#define DEVICE_ID "site1"
#define FIRMWARE_VERSION "1.0.0"

// Timing Intervals
#define SENSOR_READ_INTERVAL    10000   // 10 seconds
#define MQTT_PUBLISH_INTERVAL   300000  // 5 minutes
#define ALERT_COOLDOWN          300000  // 5 minutes between same alerts
#define SMS_CHECK_INTERVAL      5000    // 5 seconds
#define WATCHDOG_TIMEOUT_S      30      // 30 seconds

// Alert Thresholds
#define VOLTAGE_HIGH_CRITICAL   250.0   // Volts AC
#define VOLTAGE_LOW_CRITICAL    210.0
#define COMP_TEMP_CRITICAL      95.0    // Celsius
#define PRESSURE_HIGH_CRITICAL  450.0   // PSI
#define PRESSURE_LOW_CRITICAL   20.0
#define CURRENT_CRITICAL        15.0    // Amps
```

### 4.4 MQTT Payload Format

**Topic:** `heatpump/{device_id}/data`

```json
{
  "device": "site1",
  "timestamp": 1706600000000,
  "version": "1.0.0",
  "temperature": {
    "inlet": 45.2,
    "outlet": 50.1,
    "ambient": 25.0,
    "compressor": 70.5
  },
  "electrical": {
    "voltage": 230.5,
    "current": 8.5,
    "power": 1959.25
  },
  "pressure": {
    "high": 280.0,
    "low": 70.0
  },
  "status": {
    "compressor": true,
    "fan": true,
    "defrost": false
  },
  "alerts": {
    "voltage": 0,
    "compressor_temp": 0,
    "pressure_high": 0,
    "pressure_low": 0,
    "current": 0
  },
  "valid": {
    "temp_inlet": true,
    "temp_outlet": true,
    "temp_ambient": true,
    "temp_compressor": true,
    "voltage": true,
    "current": true,
    "pressure_high": true,
    "pressure_low": true
  }
}
```

### 4.5 SMS Commands

| Command | Response | Description |
|---------|----------|-------------|
| `STATUS` | Current readings + buffer status | Get system status |
| `RESET` | "Restarting device..." | Restart ESP32 |

**Example STATUS Response:**
```
HEAT PUMP STATUS
Device: site1
Temps: 45/50/25/70C
V: 230V I: 8.5A P: 1959W
Alerts: OK
Buffer: 12/100 readings
```

### 4.6 Alert Types

| Alert | Warning | Critical | Action |
|-------|---------|----------|--------|
| High Voltage | >245V | >250V | SMS to admin |
| Low Voltage | <215V | <210V | SMS to admin |
| Compressor Temp | >85°C | >95°C | SMS to admin |
| High Pressure | >400 PSI | >450 PSI | SMS to admin |
| Low Pressure | <40 PSI | <20 PSI | SMS to admin |
| Overcurrent | >12A | >15A | SMS to admin |

---

## 5. Backend API

### 5.1 Technology Stack

| Component | Technology | Version |
|-----------|------------|---------|
| Framework | FastAPI | 0.128.0 |
| Server | Uvicorn | 0.27.0 |
| Database | InfluxDB | 2.x |
| MQTT Client | paho-mqtt | 1.6.1 |
| Validation | Pydantic | 2.5.3 |

### 5.2 API Endpoints

#### Health & Stats

| Method | Endpoint | Description |
|--------|----------|-------------|
| GET | `/api/health` | Health check with service status |
| GET | `/api/stats` | System-wide statistics |

**GET /api/health Response:**
```json
{
  "status": "healthy",
  "influxdb_connected": true,
  "mqtt_connected": true,
  "timestamp": "2024-01-30T10:00:00Z"
}
```

**GET /api/stats Response:**
```json
{
  "total_readings": 15420,
  "total_alerts": 23,
  "devices_online": 1
}
```

#### Devices

| Method | Endpoint | Description |
|--------|----------|-------------|
| GET | `/api/devices` | List all devices |
| GET | `/api/devices/{device_id}/status` | Current device status |
| GET | `/api/devices/{device_id}/readings` | Historical readings |
| GET | `/api/devices/{device_id}/readings/latest` | Most recent reading |

**GET /api/devices/{device_id}/status Response:**
```json
{
  "device_id": "site1",
  "is_online": true,
  "last_seen": "2024-01-30T10:00:00Z",
  "temp_inlet": 45.2,
  "temp_outlet": 50.1,
  "temp_ambient": 25.0,
  "temp_compressor": 70.5,
  "voltage": 230.5,
  "current": 8.5,
  "power": 1959.25,
  "pressure_high": 280.0,
  "pressure_low": 70.0,
  "compressor_running": true
}
```

#### Alerts

| Method | Endpoint | Description |
|--------|----------|-------------|
| GET | `/api/alerts` | Get alert history |

**Query Parameters:**
- `device_id` (optional): Filter by device
- `hours` (1-168, default: 24): History range
- `limit` (1-500, default: 50): Max records

#### WebSocket

| Endpoint | Description |
|----------|-------------|
| `/api/ws` | Real-time data stream |

**WebSocket Message Types:**
```json
// Sensor data update
{
  "type": "sensor_data",
  "device_id": "site1",
  "data": { ... },
  "timestamp": "2024-01-30T10:00:00Z"
}

// Alert notification
{
  "type": "alert",
  "device_id": "site1",
  "alert_type": "voltage_high",
  "level": "warning",
  "value": 248.5,
  "message": "High voltage detected",
  "timestamp": "2024-01-30T10:00:00Z"
}
```

### 5.3 Environment Variables

```bash
# InfluxDB Configuration
INFLUXDB_URL=http://localhost:8086
INFLUXDB_TOKEN=your-influxdb-token
INFLUXDB_ORG=heatpump
INFLUXDB_BUCKET=sensor_data

# MQTT Configuration
MQTT_BROKER=localhost
MQTT_PORT=1883
MQTT_USER=heatpump
MQTT_PASSWORD=your-mqtt-password

# Application Settings
DEBUG=false
```

---

## 6. Data Model

### 6.1 InfluxDB Measurements

#### sensor_reading

| Field | Type | Description |
|-------|------|-------------|
| temp_inlet | float | Inlet temperature (°C) |
| temp_outlet | float | Outlet temperature (°C) |
| temp_ambient | float | Ambient temperature (°C) |
| temp_compressor | float | Compressor temperature (°C) |
| voltage | float | AC voltage (V) |
| current | float | Current draw (A) |
| power | float | Power consumption (W) |
| pressure_high | float | High-side pressure (PSI) |
| pressure_low | float | Low-side pressure (PSI) |
| compressor_running | bool | Compressor status |
| fan_running | bool | Fan status |
| defrost_active | bool | Defrost mode status |

**Tags:**
- `device_id`: Unique device identifier

#### alert

| Field | Type | Description |
|-------|------|-------------|
| value | float | Sensor value that triggered alert |
| message | string | Alert description |

**Tags:**
- `device_id`: Device that generated alert
- `alert_type`: Type of alert (voltage_high, etc.)
- `level`: Severity (warning, critical)

### 6.2 Retention Policy

| Data Type | Retention | Resolution |
|-----------|-----------|------------|
| Raw sensor data | 30 days | 10 seconds |
| Hourly aggregates | 1 year | 1 hour |
| Daily aggregates | Forever | 1 day |

---

## 7. Observability & Monitoring

### 7.1 Metrics Overview

The system provides multiple layers of observability:

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                         OBSERVABILITY STACK                                  │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                              │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐    │
│  │   Device     │  │   Backend    │  │   InfluxDB   │  │   MQTT       │    │
│  │   Health     │  │   Health     │  │   Health     │  │   Health     │    │
│  └──────┬───────┘  └──────┬───────┘  └──────┬───────┘  └──────┬───────┘    │
│         │                 │                 │                 │             │
│         ▼                 ▼                 ▼                 ▼             │
│  ┌──────────────────────────────────────────────────────────────────────┐  │
│  │                        Monitoring Dashboard                          │  │
│  │  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐  │  │
│  │  │ Device      │  │ API         │  │ Data        │  │ Alert       │  │  │
│  │  │ Status      │  │ Latency     │  │ Throughput  │  │ History     │  │  │
│  │  └─────────────┘  └─────────────┘  └─────────────┘  └─────────────┘  │  │
│  └──────────────────────────────────────────────────────────────────────┘  │
│                                                                              │
└─────────────────────────────────────────────────────────────────────────────┘
```

### 7.2 Health Check Endpoints

#### Backend Health (`GET /api/health`)

```json
{
  "status": "healthy",        // healthy | degraded | unhealthy
  "influxdb_connected": true,
  "mqtt_connected": true,
  "timestamp": "2024-01-30T10:00:00Z"
}
```

**Status Logic:**
- `healthy`: All services connected
- `degraded`: InfluxDB connected but MQTT disconnected
- `unhealthy`: InfluxDB disconnected

**Use for:** Load balancer health checks, uptime monitoring

#### System Stats (`GET /api/stats`)

```json
{
  "total_readings": 15420,
  "total_alerts": 23,
  "devices_online": 1,
  "oldest_reading": "2024-01-01T00:00:00Z",
  "newest_reading": "2024-01-30T10:00:00Z"
}
```

### 7.3 Device Monitoring

#### Device Online Status

A device is considered **online** if it has sent data within the last 2 minutes (configurable via `DEVICE_ONLINE_TIMEOUT_SECONDS`).

```python
# Check device online status
is_online = (now - last_seen).seconds < 120
```

#### Key Device Metrics

| Metric | Source | Alert Threshold |
|--------|--------|-----------------|
| Last seen | InfluxDB query | > 5 minutes = offline |
| Data publish rate | MQTT message count | < 1 per 10 min = stale |
| Buffer overflow | MQTT payload `overflow` field | Any overflow = data loss |
| GSM signal | MQTT payload (if included) | < 10% = poor signal |

### 7.4 Recommended Monitoring Stack

#### Option A: Grafana + InfluxDB (Recommended)

```
┌─────────────────────────────────────────────────────────────────┐
│                                                                 │
│  ┌─────────────┐    ┌─────────────┐    ┌─────────────────────┐ │
│  │  InfluxDB   │───►│   Grafana   │───►│  Dashboards/Alerts  │ │
│  │  (Metrics)  │    │  (Visualize)│    │   (Notification)    │ │
│  └─────────────┘    └─────────────┘    └─────────────────────┘ │
│                                                                 │
└─────────────────────────────────────────────────────────────────┘
```

**Grafana Dashboard Panels:**

1. **Device Status Overview**
   - Online/offline status per device
   - Last seen timestamp
   - Data freshness indicator

2. **Temperature Trends**
   - Line chart: 4 temperatures over time
   - Threshold lines for warning/critical

3. **Electrical Monitoring**
   - Voltage gauge with safe zone
   - Current and power trends
   - Energy consumption (kWh calculated)

4. **Pressure Monitoring**
   - High/low pressure gauges
   - Trend lines with thresholds

5. **Alert Panel**
   - Alert history table
   - Alert count by type (pie chart)
   - Time since last alert

#### Option B: Prometheus + Grafana

Add a metrics exporter endpoint:

```python
# Add to backend: /metrics endpoint
from prometheus_client import Counter, Gauge, generate_latest

# Metrics
readings_total = Counter('sensor_readings_total', 'Total sensor readings', ['device_id'])
device_online = Gauge('device_online', 'Device online status', ['device_id'])
temperature = Gauge('temperature_celsius', 'Temperature reading', ['device_id', 'sensor'])
```

### 7.5 Alerting Configuration

#### Grafana Alert Rules

**Device Offline Alert:**
```yaml
name: Device Offline
condition:
  - query: |
      from(bucket: "sensor_data")
        |> range(start: -5m)
        |> filter(fn: (r) => r["_measurement"] == "sensor_reading")
        |> count()
  - threshold: < 1
for: 5m
annotations:
  summary: "Device {{ $labels.device_id }} is offline"
  description: "No data received for 5 minutes"
```

**High Temperature Alert:**
```yaml
name: Compressor Overheating
condition:
  - query: |
      from(bucket: "sensor_data")
        |> range(start: -1m)
        |> filter(fn: (r) => r["_field"] == "temp_compressor")
        |> last()
  - threshold: > 85
annotations:
  summary: "Compressor temperature warning on {{ $labels.device_id }}"
```

**MQTT Connection Alert:**
```yaml
name: MQTT Disconnected
endpoint: /api/health
condition:
  - field: mqtt_connected
  - equals: false
for: 2m
```

### 7.6 Logging

#### Backend Logging Format

```
2024-01-30 10:00:00 - server.backend.main - INFO - Starting Heat Pump Monitor Backend...
2024-01-30 10:00:01 - server.backend.services.influxdb - INFO - InfluxDB connected: pass
2024-01-30 10:00:01 - server.backend.services.mqtt - INFO - MQTT connected to localhost:1883
2024-01-30 10:00:02 - server.backend.main - INFO - Backend startup complete
```

#### Log Levels

| Level | Usage |
|-------|-------|
| DEBUG | Detailed MQTT messages, query details |
| INFO | Startup, connections, data received |
| WARNING | Connection retries, missing data |
| ERROR | Failed writes, connection failures |

#### Recommended Log Aggregation

```
Backend Logs ──► Filebeat ──► Elasticsearch ──► Kibana
     │
     └──────────► CloudWatch Logs (AWS)
     │
     └──────────► Loki (Grafana Cloud)
```

### 7.7 Firmware Diagnostics

#### Serial Monitor Output

```
=====================================
  Smart Heat Pump Monitor
  Version: 1.0.0
=====================================
Device ID: site1
Mode: Live
Free Heap: 245632 bytes

--- Configuration Validation ---
Configuration OK

--- Watchdog Timer ---
Watchdog enabled: 30s timeout

--- GSM Initialization ---
GSM ready
Network registered: Airtel
Signal: 65%
GPRS connected
IP: 10.xxx.xxx.xxx

--- Initialization Complete ---
Starting main loop...

[MAIN] Reading sensors...
Temps: 45.2 / 50.1 / 25.0 / 70.5 C
V: 230.5V I: 8.5A P: 1959W
Pressure: 280/70 PSI
Buffer: 1/100 readings

[MAIN] MQTT publish cycle...
MQTT connected
Published 1 readings
Buffer cleared
```

#### Diagnostic SMS Commands

Send `STATUS` to get:
```
HEAT PUMP STATUS
Device: site1
Temps: 45/50/25/70C
V: 230V I: 8.5A P: 1959W
Pressure: 280/70 PSI
Alerts: OK
Buffer: 0/100 readings
Signal: 65%
Uptime: 2d 5h 30m
```

### 7.8 Monitoring Checklist

#### Daily Checks
- [ ] All devices showing "online" status
- [ ] Data freshness < 10 minutes
- [ ] No critical alerts in last 24h
- [ ] Backend health endpoint returns "healthy"

#### Weekly Checks
- [ ] Review alert history for patterns
- [ ] Check buffer overflow events (data loss)
- [ ] Verify GSM signal strength trends
- [ ] Review disk usage for InfluxDB

#### Monthly Checks
- [ ] InfluxDB retention policy working
- [ ] Backup verification
- [ ] Security updates applied
- [ ] SSL certificate expiry check

### 7.9 Uptime Monitoring

#### External Uptime Monitoring

Use services like UptimeRobot, Pingdom, or Better Uptime:

```yaml
# Uptime check configuration
- name: Heat Pump API Health
  url: https://api.yourdomain.com/api/health
  interval: 60
  expected_status: 200
  expected_content: '"status":"healthy"'
  alert_channels:
    - email: admin@example.com
    - sms: +919876543210
    - slack: #alerts
```

#### Internal Health Dashboard

Create a simple status page:

```
┌────────────────────────────────────────────────────────────────┐
│                    SYSTEM STATUS                               │
├────────────────────────────────────────────────────────────────┤
│                                                                │
│  Backend API      ● Operational                                │
│  InfluxDB         ● Operational                                │
│  MQTT Broker      ● Operational                                │
│  Device: site1    ● Online (last seen: 2 min ago)              │
│                                                                │
│  ─────────────────────────────────────────────────────         │
│  Uptime: 99.95% (last 30 days)                                 │
│  Last incident: 2024-01-15 - Network maintenance               │
│                                                                │
└────────────────────────────────────────────────────────────────┘
```

---

## 8. Deployment Guide

### 8.1 Server Requirements

| Component | Minimum | Recommended |
|-----------|---------|-------------|
| CPU | 1 core | 2 cores |
| RAM | 1 GB | 2 GB |
| Storage | 10 GB | 50 GB SSD |
| OS | Ubuntu 20.04+ | Ubuntu 22.04 LTS |

### 8.2 Docker Compose Setup

```yaml
# docker-compose.yml
version: '3.8'

services:
  influxdb:
    image: influxdb:2.7
    ports:
      - "8086:8086"
    volumes:
      - influxdb-data:/var/lib/influxdb2
    environment:
      - DOCKER_INFLUXDB_INIT_MODE=setup
      - DOCKER_INFLUXDB_INIT_USERNAME=admin
      - DOCKER_INFLUXDB_INIT_PASSWORD=adminpassword
      - DOCKER_INFLUXDB_INIT_ORG=heatpump
      - DOCKER_INFLUXDB_INIT_BUCKET=sensor_data
      - DOCKER_INFLUXDB_INIT_ADMIN_TOKEN=your-super-secret-token

  mosquitto:
    image: eclipse-mosquitto:2
    ports:
      - "1883:1883"
      - "8883:8883"
    volumes:
      - ./mosquitto/config:/mosquitto/config
      - ./mosquitto/data:/mosquitto/data
      - ./mosquitto/log:/mosquitto/log

  backend:
    build: ./server/backend
    ports:
      - "8000:8000"
    environment:
      - INFLUXDB_URL=http://influxdb:8086
      - INFLUXDB_TOKEN=your-super-secret-token
      - INFLUXDB_ORG=heatpump
      - INFLUXDB_BUCKET=sensor_data
      - MQTT_BROKER=mosquitto
      - MQTT_PORT=1883
    depends_on:
      - influxdb
      - mosquitto

  grafana:
    image: grafana/grafana:latest
    ports:
      - "3000:3000"
    volumes:
      - grafana-data:/var/lib/grafana
    environment:
      - GF_SECURITY_ADMIN_PASSWORD=admin
    depends_on:
      - influxdb

volumes:
  influxdb-data:
  grafana-data:
```

### 8.3 Mosquitto Configuration

```conf
# mosquitto/config/mosquitto.conf
listener 1883
allow_anonymous false
password_file /mosquitto/config/passwords

# TLS (optional)
listener 8883
cafile /mosquitto/config/ca.crt
certfile /mosquitto/config/server.crt
keyfile /mosquitto/config/server.key
```

### 8.4 Firmware Flashing

```bash
# Install PlatformIO CLI
pip install platformio

# Navigate to firmware directory
cd firmware

# Build and upload
pio run -t upload

# Monitor serial output
pio device monitor -b 115200
```

---

## 9. Troubleshooting

### 9.1 Device Issues

| Problem | Possible Cause | Solution |
|---------|----------------|----------|
| No data received | SIM card issue | Check balance, APN settings |
| Intermittent data | Poor signal | Relocate antenna, check carrier |
| Wrong readings | Sensor wiring | Verify connections, check calibration |
| Device restarts | Watchdog timeout | Check for blocking code, increase timeout |
| SMS not working | Wrong phone format | Use international format (+91...) |

### 9.2 Backend Issues

| Problem | Possible Cause | Solution |
|---------|----------------|----------|
| Health check failing | Service down | Check docker-compose logs |
| No MQTT data | Wrong broker address | Verify MQTT_BROKER env var |
| InfluxDB write fails | Token invalid | Regenerate token, update config |
| WebSocket disconnects | Timeout | Implement ping/pong in client |

### 9.3 Common Commands

```bash
# Check backend logs
docker-compose logs -f backend

# Check MQTT messages
mosquitto_sub -h localhost -u heatpump -P password -t "heatpump/#" -v

# Query InfluxDB
influx query 'from(bucket:"sensor_data") |> range(start:-1h) |> limit(n:10)'

# Test API
curl http://localhost:8000/api/health
curl http://localhost:8000/api/devices
curl http://localhost:8000/api/devices/site1/readings/latest
```

---

## Appendix A: Quick Reference

### API Endpoints Summary

| Endpoint | Method | Description |
|----------|--------|-------------|
| `/api/health` | GET | Health check |
| `/api/stats` | GET | System statistics |
| `/api/devices` | GET | List devices |
| `/api/devices/{id}/status` | GET | Device status |
| `/api/devices/{id}/readings` | GET | Historical data |
| `/api/devices/{id}/readings/latest` | GET | Latest reading |
| `/api/alerts` | GET | Alert history |
| `/api/ws` | WebSocket | Real-time stream |

### MQTT Topics

| Topic | Direction | Content |
|-------|-----------|---------|
| `heatpump/{id}/data` | Device → Server | Sensor readings |
| `heatpump/{id}/status/online` | Device → Server | Online status |
| `heatpump/{id}/alerts` | Device → Server | Alert events |

### SMS Commands

| Command | Response |
|---------|----------|
| STATUS | Current readings and status |
| RESET | Device restart |

---

*Document Version: 1.0*
*Last Updated: January 2026*
*Project: Smart Heat Pump Monitoring System - Stage 1*
