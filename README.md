# Smart Heat Pump Monitoring System - Project Brief

## Overview

Build a prototype IoT monitoring system for a smart heat pump that monitors various sensors and sends alerts to an admin. The system operates in remote locations **without WiFi/Internet**, so it relies on **GSM cellular (SMS + GPRS)** for communication.

---

## Hardware Components

### Microcontroller
- **ESP32 WROOM** (already owned)
- 2 cores, WiFi + Bluetooth built-in
- Arduino IDE 2.3.7 on macOS for development


### GSM Module
- **SIM800C Module** (GSM/GPRS, 2G)
- Development board with onboard voltage regulator
- Glue stick antenna included
- Micro SIM slot
- Quad-band: 850/900/1800/1900 MHz
- **Note:** 2G only - works with Airtel/Vi in India, NOT Jio (4G only)
- Communicates via UART (AT commands)

### SIM Card
- Airtel or Vodafone-Idea (Vi) prepaid SIM
- Micro SIM size
- Requires: SMS pack + GPRS data pack
- PIN lock must be disabled before use

### Sensors (to be connected)
| Sensor | Purpose | Quantity |
|--------|---------|----------|
| 10K NTC Thermistor | Temperature (inlet, outlet, ambient, compressor) | 4 |
| ZMPT101B | AC Voltage monitoring | 1 |
| ACS712-20A | Current monitoring | 1 |
| Pressure Transducer (0-500 PSI) | High/Low side pressure | 2 (optional) |

### Power
- ESP32: 5V via USB
- SIM800C: Development board has onboard regulator, accepts 5V input
- For production: Consider 18650 LiPo battery backup

---

## Wiring: ESP32 to SIM800C

```
SIM800C Board          ESP32
─────────────          ─────
5V (or VCC) ◄───────── VIN (5V)
GND ◄──────────────────GND
TXD ───────────────────► GPIO16 (RX2)
RXD ◄────────────────── GPIO17 (TX2)
```

---

## System Architecture

```
┌─────────────────────────────────────────────────────────────────────────┐
│                           HEAT PUMP SITE                                 │
│                         (No WiFi/Internet)                               │
│                                                                          │
│   ┌──────────────┐         ┌──────────────┐                             │
│   │    ESP32     │◄───────►│   SIM800C    │                             │
│   │    WROOM     │  UART   │   Module     │                             │
│   └──────────────┘         └──────────────┘                             │
│         │                        │                                       │
│    Sensors                       ├─── SMS ──────────► 📱 Admin Phone    │
│    - Temperature (x4)            │    (Critical alerts, immediate)       │
│    - Voltage                     │                                       │
│    - Current                     └─── GPRS ─────────► ☁️ MQTT Broker    │
│    - Pressure (x2)                    (Data logging, when available)     │
│                                                                          │
└─────────────────────────────────────────────────────────────────────────┘
```

---

## Communication Strategy

### 1. SMS (Primary - Always works)
- **Critical alerts**: Sent immediately via SMS
- **Admin commands**: Admin can text "STATUS" to get current readings
- **No internet required**

### 2. MQTT over GPRS (Secondary - When available)
- **Data logging**: Periodic sensor data upload
- **Buffer locally**: Store readings when GPRS unavailable
- **Upload when possible**: Send buffered data when connection available

---

## Alert Thresholds

| Condition | Warning | Critical | Action |
|-----------|---------|----------|--------|
| High Voltage | > 245V | > 250V | SMS Alert |
| Low Voltage | < 215V | < 210V | SMS Alert |
| Compressor Temp | > 85°C | > 95°C | SMS Alert |
| High Pressure | > 400 PSI | > 450 PSI | SMS Alert |
| Low Pressure | < 40 PSI | < 20 PSI | SMS Alert |
| Overcurrent | > 12A | > 15A | SMS Alert |

---

## Server Architecture (Single Server)

All components on one server (VPS or Raspberry Pi):

```
┌──────────────────────────────────────┐
│            SERVER                    │
│                                      │
│  ┌──────────────┐  ┌──────────────┐ │
│  │  Mosquitto   │  │   Web App    │ │
│  │  (MQTT)      │  │  (Dashboard) │ │
│  │  Port 1883   │  │   Port 80    │ │
│  └──────────────┘  └──────────────┘ │
│         │                 ▲         │
│         ▼                 │         │
│  ┌────────────────────────┐         │
│  │  Database (PostgreSQL/ │         │
│  │  SQLite/InfluxDB)      │         │
│  │      Port 5432         │         │
│  └────────────────────────┘         │
│                                      │
└──────────────────────────────────────┘
```

### Development Stages

| Stage | Dashboard | Description |
|-------|-----------|-------------|
| **Stage 1** | Web Page | Simple HTML/CSS/JS dashboard hosted on server |
| **Stage 2** | Flutter App | Mobile app (iOS/Android) for admin monitoring |

### Server Options
- DigitalOcean Droplet (~₹400/month)
- AWS EC2 t3.micro (free tier)
- Oracle Cloud (always free tier)
- Raspberry Pi 4 (local/home)

---

## MQTT Topic Structure

```
heatpump/
├── {device_id}/
│   ├── sensors/
│   │   ├── temperature/inlet
│   │   ├── temperature/outlet
│   │   ├── temperature/ambient
│   │   ├── temperature/compressor
│   │   ├── voltage
│   │   ├── current
│   │   ├── power
│   │   ├── pressure/high
│   │   └── pressure/low
│   ├── status/
│   │   ├── compressor
│   │   ├── fan
│   │   ├── defrost
│   │   └── online
│   └── alerts/
```

---

## Required Libraries (Arduino IDE)

| Library | Purpose | Install via |
|---------|---------|-------------|
| TinyGSM | SIM800C communication | Library Manager |
| ArduinoJson | JSON parsing | Library Manager |
| PubSubClient | MQTT client | Library Manager |

---

## Firmware Requirements

### Core Features
1. **Sensor Reading**
   - Read all sensors every 10 seconds
   - Calculate derived values (power, COP)

2. **Alert System**
   - Check thresholds after each reading
   - Send SMS immediately for critical alerts
   - Cooldown period to avoid SMS spam (e.g., 5 min between same alert)

3. **Data Buffering**
   - Store readings in ESP32 memory (or SPIFFS)
   - Buffer up to 100 readings when offline

4. **MQTT Publishing**
   - Attempt GPRS connection every 5 minutes
   - If connected: publish buffered data, clear buffer
   - If failed: keep buffer, try later

5. **SMS Commands**
   - Respond to "STATUS" SMS with current readings
   - Respond to "RESET" SMS to restart device

### Code Structure
```
/esp32_firmware/
├── heat_pump_monitor.ino    # Main file
├── config.h                 # Configuration (phone numbers, thresholds)
├── sensors.h                # Sensor reading functions
├── gsm.h                    # SIM800C SMS & GPRS functions
├── mqtt.h                   # MQTT publishing
└── alerts.h                 # Alert logic
```

---

## Configuration Parameters

```cpp
// Admin phone number (with country code)
#define ADMIN_PHONE "+919876543210"

// MQTT Broker (for when GPRS is available)
#define MQTT_BROKER "your-server-ip.com"
#define MQTT_PORT 1883
#define MQTT_USER "heatpump"
#define MQTT_PASS "password"

// Device ID (unique per heat pump)
#define DEVICE_ID "site1"

// Intervals
#define SENSOR_READ_INTERVAL 10000      // 10 seconds
#define MQTT_PUBLISH_INTERVAL 300000    // 5 minutes
#define ALERT_COOLDOWN 300000           // 5 minutes between same alerts

// SIM800C UART
#define SIM800_TX 17
#define SIM800_RX 16
#define SIM800_BAUD 9600
```

---

## Development Environment

- **IDE**: Arduino IDE 2.3.7
- **OS**: macOS
- **Board Selection**: ESP32 Dev Module
- **Baud Rate**: 115200 (Serial Monitor)
- **Flash Mode**: DIO (if QIO causes issues)

### Board Manager URL
```
https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
```

---

## Testing Sequence

1. **Test ESP32 alone** - Chip ID script (done ✓)
2. **Test SIM800C** - AT commands, network registration
3. **Test SMS sending** - Send test SMS to admin
4. **Test SMS receiving** - Respond to STATUS command
5. **Test sensors** - Read and display values
6. **Test GPRS** - Connect to internet
7. **Test MQTT** - Publish to broker
8. **Integration** - Full system test

---

## Important Notes

1. **2G Sunset**: SIM800C is 2G only. Networks may shut down in 2-3 years in India. For production, consider upgrading to A7670C (4G) module.

2. **Power Spikes**: SIM800C draws 2A peaks during transmission. Ensure adequate power supply with capacitors.

3. **Antenna Required**: SIM800C won't register on network without antenna attached.

4. **SIM Activation**: Activate SIM in a regular phone first, disable PIN lock.

5. **SMS Costs**: Each SMS costs money. Implement cooldown to avoid excessive alerts.

---

## Deliverables Expected

### Stage 1 (Current)

1. **ESP32 Firmware** - Complete Arduino sketch with:
   - Sensor reading (simulated for now, real pins defined)
   - SMS alerts via SIM800C
   - SMS command handling (STATUS, RESET)
   - MQTT publishing over GPRS
   - Local data buffering
   - Configurable thresholds

2. **Server Backend** - Node.js/Python with:
   - Mosquitto MQTT broker
   - Database (PostgreSQL/SQLite/InfluxDB)
   - REST API for dashboard

3. **Web Dashboard** - Simple HTML/CSS/JS page:
   - Real-time sensor display
   - Alert history
   - System status (compressor, fan, etc.)
   - Basic controls

4. **Wiring Diagram** - Pin connections for all components

5. **Testing Scripts** - Simple sketches to test each component individually

### Stage 2 (Future)

1. **Flutter Mobile App** - iOS/Android app with:
   - Real-time monitoring dashboard
   - Push notifications for alerts
   - Historical data graphs
   - Multi-site support (multiple heat pumps)
   - Admin controls

---

## Current Status

- [x] ESP32 working (Chip ID test passed)
- [x] SIM800C module acquired
- [ ] SIM800C wiring and testing
- [ ] SMS sending/receiving
- [ ] Sensor integration
- [ ] MQTT over GPRS
- [ ] Server setup (MQTT broker + database)
- [ ] Web dashboard (Stage 1)
- [ ] Flutter app (Stage 2 - future)
