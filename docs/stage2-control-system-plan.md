# Stage 2: Heat Pump Control System - Detailed Implementation Plan

## Table of Contents
1. [Executive Summary](#1-executive-summary)
2. [System Architecture](#2-system-architecture)
3. [Hardware Requirements & Parts List](#3-hardware-requirements--parts-list)
4. [Wiring & Connections](#4-wiring--connections)
5. [Firmware Implementation](#5-firmware-implementation)
6. [Backend Implementation](#6-backend-implementation)
7. [Mobile App Implementation](#7-mobile-app-implementation)
8. [Security Design](#8-security-design)
9. [Testing & Verification](#9-testing--verification)
10. [Implementation Timeline](#10-implementation-timeline)

---

## 1. Executive Summary

### Current State (Stage 1)
- ESP32 + SIM800C GSM module for remote monitoring
- Sensors: 4x temperature (NTC), voltage/current, 2x pressure
- Data transmitted via MQTT over GPRS
- SMS alerts for critical conditions
- FastAPI backend with InfluxDB storage
- **Monitoring only - no control capability**

### Stage 2 Goals
- Full remote control via mobile app
- Control modes: Heating, Cooling, Fan Only, Off
- Fan speed control: Low, Medium, High, Auto
- Defrost trigger and Emergency Stop
- Safe, reliable operation with hardware interlocks
- Audit trail for all commands

---

## 2. System Architecture

### Data & Command Flow

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                              STAGE 2 ARCHITECTURE                           │
└─────────────────────────────────────────────────────────────────────────────┘

┌──────────────┐     GPRS/4G      ┌──────────────┐      HTTP/WS     ┌─────────────┐
│  Heat Pump   │◄────────────────►│   Backend    │◄────────────────►│ Mobile App  │
│  Controller  │    MQTT/TLS      │   Server     │                  │  (Flutter)  │
└──────────────┘                  └──────────────┘                  └─────────────┘
       │                                 │
       │ GPIO                            │
       ▼                                 ▼
┌──────────────┐                  ┌──────────────┐
│ Relay Module │                  │  PostgreSQL  │
│ (8-channel)  │                  │  + InfluxDB  │
└──────────────┘                  └──────────────┘
       │
       │ Contactor/Relays
       ▼
┌──────────────────────────────────────────────┐
│              HEAT PUMP UNIT                  │
│  ┌──────────┐  ┌─────┐  ┌─────────────────┐  │
│  │Compressor│  │ Fan │  │ Reversing Valve │  │
│  └──────────┘  └─────┘  └─────────────────┘  │
└──────────────────────────────────────────────┘
```

### MQTT Topic Structure

```
# Existing (Stage 1 - Device → Server)
heatpump/{device_id}/data           # Sensor readings (every 5 min)
heatpump/{device_id}/status/online  # Online status (retained)
heatpump/{device_id}/alerts         # Alert notifications

# New (Stage 2 - Bidirectional)
heatpump/{device_id}/commands       # Server → Device (commands)
heatpump/{device_id}/responses      # Device → Server (command results)
heatpump/{device_id}/control/state  # Device → Server (current state, retained)
```

---

## 3. Hardware Requirements & Parts List

### 3.1 Relay Control Module

| Component | Model | Specifications | Qty | Est. Price (INR) | Notes |
|-----------|-------|----------------|-----|------------------|-------|
| **Relay Module** | SainSmart 8-Channel 5V Relay | 10A @ 250VAC per channel, optocoupler isolation | 1 | ₹450-600 | Primary control outputs |
| **Alternative** | Waveshare 8-CH Relay Expansion | Same specs, screw terminals | 1 | ₹550-700 | Better terminal quality |

**Why 8 channels?**
| Channel | Function | Load Type |
|---------|----------|-----------|
| Relay 1 | Compressor Enable | Contactor coil (24VAC) |
| Relay 2 | Fan Low Speed | Direct or contactor |
| Relay 3 | Fan Medium Speed | Direct or contactor |
| Relay 4 | Fan High Speed | Direct or contactor |
| Relay 5 | Cooling Mode Valve | 24VAC solenoid |
| Relay 6 | Heating Mode Valve | 24VAC solenoid |
| Relay 7 | Defrost Trigger | 24VAC signal |
| Relay 8 | Emergency Stop | NC contact in safety chain |

### 3.2 Contactor for Compressor

**Important:** Never switch compressor motor (15-20A inductive) directly with relay module. Use a contactor.

| Component | Model | Specifications | Qty | Est. Price (INR) |
|-----------|-------|----------------|-----|------------------|
| **AC Contactor** | Schneider LC1D09 | 9A AC-3, 24VAC coil | 1 | ₹800-1200 |
| **Alternative** | Siemens 3RT1015 | 7A AC-3, 24VAC coil | 1 | ₹900-1300 |
| **Budget Option** | Havells?"GHNCAXN009M3 | 9A, 24VAC coil | 1 | ₹500-700 |

**Wiring:** Relay 1 (5V logic) → Contactor coil (24VAC) → Compressor power

### 3.3 Hardware Watchdog Timer

External watchdog ensures ESP32 reset if firmware hangs.

| Component | Model | Specifications | Qty | Est. Price (INR) |
|-----------|-------|----------------|-----|------------------|
| **Watchdog IC** | TPL5010 | Nano-power timer, 1s-7200s programmable | 1 | ₹150-200 |
| **Breakout Board** | Adafruit TPL5010 Breakout | With headers, easy prototyping | 1 | ₹350-450 |

**Configuration:** Set to 200ms timeout via resistor. ESP32 must pulse GPIO13 every 100ms to prevent reset.

### 3.4 Power Supply

| Component | Model | Specifications | Qty | Est. Price (INR) |
|-----------|-------|----------------|-----|------------------|
| **5V Power Supply** | Hi-Link HLK-PM01 | 5V 600mA, AC-DC module | 1 | ₹150-200 |
| **24VAC Transformer** | (Use existing from heat pump) | For contactor/valve coils | - | - |
| **Capacitors** | Electrolytic | 100uF, 470uF for filtering | 2 | ₹10-20 |

### 3.5 Protection Components

| Component | Model | Specifications | Qty | Est. Price (INR) |
|-----------|-------|----------------|-----|------------------|
| **Flyback Diodes** | 1N4007 | 1000V 1A, for relay coils | 8 | ₹40 |
| **Current Limiting Resistors** | 1K ohm 1/4W | Between ESP32 and relay inputs | 8 | ₹20 |
| **Fuses** | Glass tube 10A 250V | For each relay output | 8 | ₹80 |
| **Fuse Holders** | Panel mount | For easy replacement | 8 | ₹160 |
| **TVS Diodes** | P6KE18CA | Transient voltage suppression | 4 | ₹60 |
| **Varistor** | 14D471K | 300V surge protection on AC input | 1 | ₹30 |

### 3.6 Connectors & Enclosure

| Component | Model | Specifications | Qty | Est. Price (INR) |
|-----------|-------|----------------|-----|------------------|
| **Terminal Blocks** | Phoenix Contact or generic | 2-position, 5.08mm pitch | 10 | ₹200 |
| **DIN Rail** | Standard 35mm | For mounting | 1 | ₹100 |
| **Enclosure** | IP65 Junction Box | 200x150x100mm minimum | 1 | ₹400-600 |
| **Cable Glands** | PG9, PG11 | For cable entry | 6 | ₹120 |
| **Jumper Wires** | 22AWG stranded | Internal wiring | 1 set | ₹150 |

### 3.7 Complete Parts List Summary

| Category | Estimated Cost (INR) |
|----------|---------------------|
| Relay Module | ₹500 |
| Contactor | ₹800 |
| Watchdog Timer | ₹350 |
| Power Supply | ₹200 |
| Protection Components | ₹400 |
| Connectors & Enclosure | ₹700 |
| Miscellaneous (cables, solder, etc.) | ₹300 |
| **Total Hardware** | **₹3,250** |

### 3.8 Recommended Suppliers (India)

- **Robu.in** - Good selection of relay modules, ESP32
- **Amazon.in** - Contactors, enclosures
- **Electronicspices.com** - Components, terminal blocks
- **Evelta.com** - Quality modules, breakout boards
- **Mouser/Digikey** - Industrial-grade components (for production)

---

## 4. Wiring & Connections

### 4.1 ESP32 GPIO Pin Allocation

**Existing Pins (Stage 1 - Do Not Change):**
| GPIO | Function | Notes |
|------|----------|-------|
| 16 | GSM RX | Serial to SIM800C |
| 17 | GSM TX | Serial to SIM800C |
| 34 | Temp Inlet | ADC1_CH6 |
| 35 | Temp Outlet | ADC1_CH7 |
| 32 | Temp Ambient | ADC1_CH4 |
| 33 | Temp Compressor | ADC1_CH5 |
| 36 | Voltage Sensor | ADC1_CH0 |
| 39 | Current Sensor | ADC1_CH3 |
| 25 | Pressure High | ADC2_CH8 |
| 26 | Pressure Low | ADC2_CH9 |
| 2 | Status LED | Built-in |

**New Pins (Stage 2 - Control Outputs):**
| GPIO | Function | Active Level | Notes |
|------|----------|--------------|-------|
| 4 | Relay 1: Compressor | LOW | Safe output pin |
| 5 | Relay 2: Fan Low | LOW | Has internal pull-up |
| 18 | Relay 3: Fan Medium | LOW | VSPI CLK, OK for GPIO |
| 19 | Relay 4: Fan High | LOW | VSPI MISO, OK for GPIO |
| 21 | Relay 5: Cooling Valve | LOW | I2C SDA, usable as GPIO |
| 22 | Relay 6: Heating Valve | LOW | I2C SCL, usable as GPIO |
| 23 | Relay 7: Defrost | LOW | VSPI MOSI, OK for GPIO |
| 27 | Relay 8: E-Stop | LOW | Emergency stop (NC) |
| 13 | Watchdog Pulse | - | External TPL5010 DONE pin |
| 14 | Control Status LED | HIGH | Secondary status |

**Pins to Avoid:**
- GPIO 0: Boot mode
- GPIO 1, 3: UART0 (Serial debug)
- GPIO 6-11: Internal flash
- GPIO 12: Boot strapping pin

### 4.2 Relay Module Wiring Diagram

```
                                 ESP32 DevKit
                              ┌───────────────┐
                              │    ┌─────┐    │
                              │    │ USB │    │
                              │    └──┬──┘    │
                              │       │       │
                         3V3 ─┤●     EN      ●├─ GND
                              │●     VP  D23 ●├──────────[1K]───► Relay 7 IN
                              │●     VN  D22 ●├──────────[1K]───► Relay 6 IN
                        D34 ──┤●        D21 ●├──────────[1K]───► Relay 5 IN
                        D35 ──┤●  ESP32 D19 ●├──────────[1K]───► Relay 4 IN
                        D32 ──┤●        D18 ●├──────────[1K]───► Relay 3 IN
                        D33 ──┤●         D5 ●├──────────[1K]───► Relay 2 IN
                        D25 ──┤●        TX0 ●├─
                        D26 ──┤●        RX0 ●├─
                        D27 ──┤●────────────────────────[1K]───► Relay 8 IN (E-Stop)
                        D14 ──┤●────────────────────────────────► Control LED (+)
                        D12 ──┤●         D4 ●├──────────[1K]───► Relay 1 IN
                        GND ──┤●         D2 ●├──────────────────► Status LED
                        D13 ──┤●  D15   D15 ●├─
                              │●            ●├─
                         VIN ─┤●            ●├─
                              └───────────────┘


                            8-Channel Relay Module
                    ┌─────────────────────────────────────┐
                    │  VCC  GND  IN1  IN2  IN3  ...  IN8  │
                    │   │    │    │    │    │         │   │
                    └───┼────┼────┼────┼────┼─────────┼───┘
                        │    │    │    │    │         │
    5V Supply ──────────┘    │    │    │    │         │
                             │    │    │    │         │
    ESP32 GND ───────────────┘    │    │    │         │
                                  │    │    │         │
    GPIO4 (via 1K) ───────────────┘    │    │         │
    GPIO5 (via 1K) ────────────────────┘    │         │
    GPIO18 (via 1K) ────────────────────────┘         │
    ...                                               │
    GPIO27 (via 1K) ──────────────────────────────────┘


                            Contactor Wiring (Compressor)
    ┌─────────────────────────────────────────────────────────────────────┐
    │                                                                      │
    │   Relay 1 COM ────── 24VAC (from heat pump transformer)             │
    │                                                                      │
    │   Relay 1 NO ─────── Contactor Coil A1                              │
    │                           │                                          │
    │                      Contactor Coil A2 ────── 24VAC Neutral         │
    │                           │                                          │
    │                      ┌────┴────┐                                     │
    │                      │Contactor│                                     │
    │                      │ LC1D09  │                                     │
    │                      │ ┌─────┐ │                                     │
    │   L1 (230VAC) ───────┤ │     │ ├────── Compressor L                 │
    │   L2 (230VAC) ───────┤ │     │ ├────── Compressor N                 │
    │   L3 (if 3-ph) ──────┤ │     │ ├────── Compressor (3-ph)            │
    │                      └─┴─────┴─┘                                     │
    └─────────────────────────────────────────────────────────────────────┘
```

### 4.3 Hardware Watchdog Circuit

```
                                TPL5010 Breakout
                              ┌─────────────────┐
    5V ───────────────────────┤ VDD         OUT ├─
                              │                 │
    ESP32 EN ─────────────────┤ RSTn       DONE ├───────── ESP32 GPIO13
                              │                 │
    GND ──────────────────────┤ GND     DELAY/M ├───┬─── GND (200ms timeout)
                              └─────────────────┘   │
                                                    └─── Use resistor for
                                                         custom timeout
```

**Firmware requirement:** Call `feedWatchdog()` (pulse GPIO13 HIGH for 1ms) every 100ms in main loop.

### 4.4 Emergency Stop Wiring (Fail-Safe)

```
                        EMERGENCY STOP CIRCUIT
    ┌──────────────────────────────────────────────────────────────────┐
    │                                                                   │
    │   The E-Stop relay uses NC (Normally Closed) contacts.           │
    │   When ESP32 is OFF or crashed, relay is de-energized,           │
    │   and NC contact OPENS, breaking the safety chain.               │
    │                                                                   │
    │   24VAC ──┬── Relay 8 COM                                        │
    │           │                                                       │
    │           └── Relay 8 NC ──── Safety Chain ──── All Contactors   │
    │                   │                                               │
    │              (opens when                                          │
    │               de-energized)                                       │
    │                                                                   │
    │   Normal Operation: GPIO27 HIGH → Relay energized → NC closed    │
    │   E-Stop Active:    GPIO27 LOW  → Relay de-energized → NC open   │
    │   Power Failure:    No power    → Relay de-energized → NC open   │
    │                                                                   │
    └──────────────────────────────────────────────────────────────────┘
```

---

## 5. Firmware Implementation

### 5.1 New Files to Create

```
firmware/
├── control.h           # Control interface declarations
├── control.cpp         # Relay control implementation
├── interlocks.h        # Safety interlock definitions
├── interlocks.cpp      # Interlock logic
├── command_handler.h   # MQTT command parsing
└── command_handler.cpp # Command execution
```

### 5.2 Configuration Additions (`config.h`)

```cpp
// =============================================================================
// PIN DEFINITIONS - Control Outputs (Stage 2)
// =============================================================================
#define PIN_RELAY_COMPRESSOR   4     // Relay 1 - Compressor enable
#define PIN_RELAY_FAN_LOW      5     // Relay 2 - Fan low speed
#define PIN_RELAY_FAN_MED      18    // Relay 3 - Fan medium speed
#define PIN_RELAY_FAN_HIGH     19    // Relay 4 - Fan high speed
#define PIN_RELAY_VALVE_COOL   21    // Relay 5 - Cooling mode valve
#define PIN_RELAY_VALVE_HEAT   22    // Relay 6 - Heating mode valve
#define PIN_RELAY_DEFROST      23    // Relay 7 - Defrost trigger
#define PIN_RELAY_ESTOP        27    // Relay 8 - Emergency stop (NC)
#define PIN_WATCHDOG_PULSE     13    // External watchdog feed
#define PIN_CONTROL_LED        14    // Control status LED

// Relay active level (most modules are active-low)
#define RELAY_ACTIVE    LOW
#define RELAY_INACTIVE  HIGH

// =============================================================================
// CONTROL TIMING (milliseconds)
// =============================================================================
#define COMPRESSOR_RESTART_DELAY_MS  180000UL  // 3 minutes between stop/start
#define FAN_PRERUN_DELAY_MS          30000UL   // Fan runs 30s before compressor
#define FAN_POSTRUN_DELAY_MS         60000UL   // Fan runs 60s after compressor stops
#define MODE_TRANSITION_DELAY_MS     5000UL    // Delay between mode changes
#define WATCHDOG_FEED_INTERVAL_MS    100UL     // Feed external watchdog every 100ms
#define COMMAND_TIMEOUT_MS           30000UL   // Command expires after 30s

// =============================================================================
// MQTT TOPICS - Control (Stage 2)
// =============================================================================
#define MQTT_TOPIC_COMMANDS      "heatpump/" DEVICE_ID "/commands"
#define MQTT_TOPIC_RESPONSES     "heatpump/" DEVICE_ID "/responses"
#define MQTT_TOPIC_CONTROL_STATE "heatpump/" DEVICE_ID "/control/state"
```

### 5.3 Type Definitions (`types.h` additions)

```cpp
// =============================================================================
// CONTROL TYPES (Stage 2)
// =============================================================================

/**
 * @brief Operating modes for heat pump
 */
enum OperatingMode {
    MODE_OFF = 0,
    MODE_COOLING,
    MODE_HEATING,
    MODE_FAN_ONLY,
    MODE_DEFROST,
    MODE_EMERGENCY_STOP
};

/**
 * @brief Fan speed levels
 */
enum FanSpeed {
    FAN_OFF = 0,
    FAN_LOW,
    FAN_MEDIUM,
    FAN_HIGH,
    FAN_AUTO
};

/**
 * @brief Command execution result codes
 */
enum CommandResult {
    CMD_SUCCESS = 0,
    CMD_REJECTED_INTERLOCK,      // Safety interlock prevented execution
    CMD_REJECTED_COOLDOWN,       // Compressor restart delay active
    CMD_REJECTED_INVALID,        // Invalid command or parameters
    CMD_REJECTED_SAFETY,         // Sensor values out of safe range
    CMD_QUEUED,                  // Command queued for later execution
    CMD_FAILED                   // Execution failed
};

/**
 * @brief Current control state
 */
struct ControlState {
    OperatingMode mode;
    FanSpeed fanSpeed;
    bool compressorRunning;
    bool defrostActive;
    bool emergencyStop;

    unsigned long compressorStopTime;   // For restart delay tracking
    unsigned long fanStartTime;         // For pre-run tracking
    unsigned long lastStateChange;      // Timestamp of last change

    ControlState() :
        mode(MODE_OFF), fanSpeed(FAN_OFF),
        compressorRunning(false), defrostActive(false), emergencyStop(false),
        compressorStopTime(0), fanStartTime(0), lastStateChange(0) {}
};

/**
 * @brief MQTT command types
 */
enum MQTTCommandType {
    MQTT_CMD_NONE = 0,
    MQTT_CMD_SET_MODE,
    MQTT_CMD_SET_FAN,
    MQTT_CMD_DEFROST,
    MQTT_CMD_EMERGENCY_STOP,
    MQTT_CMD_STATUS,
    MQTT_CMD_UNKNOWN
};

/**
 * @brief Parsed command from MQTT
 */
struct Command {
    char commandId[40];              // UUID from server
    MQTTCommandType type;
    OperatingMode targetMode;
    FanSpeed targetFan;
    bool emergencyStopActivate;
    unsigned long receivedTime;

    Command() : type(MQTT_CMD_NONE), targetMode(MODE_OFF),
                targetFan(FAN_OFF), emergencyStopActivate(false),
                receivedTime(0) {
        commandId[0] = '\0';
    }
};
```

### 5.4 Safety Interlocks

| Interlock | Condition | Action |
|-----------|-----------|--------|
| **Compressor Restart Delay** | < 3 min since last stop | Reject compressor start |
| **Fan Pre-Run** | Compressor requested, fan off | Start fan, delay compressor 30s |
| **Fan Post-Run** | Compressor stopped | Keep fan running 60s |
| **High Pressure Cutout** | pressure_high > 450 PSI | Stop compressor immediately |
| **Low Pressure Cutout** | pressure_low < 20 PSI | Stop compressor immediately |
| **Compressor Temp Cutout** | temp_compressor > 95°C | Stop compressor immediately |
| **Voltage Protection** | voltage < 210V or > 250V | Disable all outputs |
| **Overcurrent Protection** | current > 15A | Stop compressor, alert |

### 5.5 Command Protocol

**Incoming Command (JSON):**
```json
{
    "command_id": "550e8400-e29b-41d4-a716-446655440000",
    "command": "set_mode",
    "params": {
        "mode": "heating"
    },
    "timestamp": 1706600000
}
```

**Supported Commands:**
| Command | Parameters | Description |
|---------|------------|-------------|
| `set_mode` | `mode`: "off", "cooling", "heating", "fan_only" | Set operating mode |
| `set_fan` | `speed`: "off", "low", "medium", "high", "auto" | Set fan speed |
| `defrost` | none | Trigger defrost cycle |
| `emergency_stop` | `activate`: true/false | E-stop on/off |
| `status` | none | Request full status |

**Response (JSON):**
```json
{
    "command_id": "550e8400-e29b-41d4-a716-446655440000",
    "result": "success",
    "error_code": null,
    "state": {
        "mode": "heating",
        "fan_speed": "high",
        "compressor": true,
        "defrost": false,
        "emergency_stop": false
    },
    "timestamp": 1706600001
}
```

---

## 6. Backend Implementation

### 6.1 Database Schema (PostgreSQL)

Add PostgreSQL alongside existing InfluxDB:

```sql
-- Users table
CREATE TABLE users (
    id SERIAL PRIMARY KEY,
    email VARCHAR(255) UNIQUE NOT NULL,
    password_hash VARCHAR(255) NOT NULL,
    name VARCHAR(100),
    is_active BOOLEAN DEFAULT true,
    is_admin BOOLEAN DEFAULT false,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

-- Devices table
CREATE TABLE devices (
    id SERIAL PRIMARY KEY,
    device_id VARCHAR(50) UNIQUE NOT NULL,
    name VARCHAR(100),
    location VARCHAR(200),
    description TEXT,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

-- User-Device permissions
CREATE TABLE user_device_permissions (
    id SERIAL PRIMARY KEY,
    user_id INTEGER REFERENCES users(id) ON DELETE CASCADE,
    device_id VARCHAR(50) REFERENCES devices(device_id) ON DELETE CASCADE,
    can_view BOOLEAN DEFAULT true,
    can_control BOOLEAN DEFAULT false,
    can_admin BOOLEAN DEFAULT false,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    UNIQUE(user_id, device_id)
);

-- Commands table
CREATE TABLE commands (
    id SERIAL PRIMARY KEY,
    command_id UUID UNIQUE NOT NULL,
    device_id VARCHAR(50) REFERENCES devices(device_id),
    command_type VARCHAR(50) NOT NULL,
    parameters JSONB,

    -- Status tracking
    status VARCHAR(20) DEFAULT 'pending',  -- pending, sent, acknowledged, executed, failed, expired

    -- Metadata
    source VARCHAR(50),  -- mobile_app, web_dashboard, api, scheduled
    user_id INTEGER REFERENCES users(id),

    -- Timestamps
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    sent_at TIMESTAMP,
    acknowledged_at TIMESTAMP,
    executed_at TIMESTAMP,

    -- Response
    response_code VARCHAR(50),
    response_message TEXT,

    -- Expiration
    expires_at TIMESTAMP,
    retry_count INTEGER DEFAULT 0
);

CREATE INDEX idx_commands_device_status ON commands(device_id, status);
CREATE INDEX idx_commands_created ON commands(created_at DESC);

-- Audit log
CREATE TABLE command_audit_log (
    id SERIAL PRIMARY KEY,
    command_id UUID,
    event_type VARCHAR(50) NOT NULL,
    event_data JSONB,
    ip_address INET,
    user_agent TEXT,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);
```

### 6.2 New API Endpoints

**File: `server/backend/api/commands.py`**

| Method | Endpoint | Description |
|--------|----------|-------------|
| POST | `/api/devices/{device_id}/command` | Send command to device |
| GET | `/api/devices/{device_id}/command/{command_id}` | Get command status |
| GET | `/api/devices/{device_id}/commands` | List command history |
| GET | `/api/devices/{device_id}/control/state` | Get current control state |

**Authentication Endpoints (new):**

| Method | Endpoint | Description |
|--------|----------|-------------|
| POST | `/api/auth/login` | Login, returns JWT |
| POST | `/api/auth/refresh` | Refresh access token |
| POST | `/api/auth/register` | Register new user |
| GET | `/api/auth/me` | Get current user info |

### 6.3 MQTT Service Extensions

**Add to `server/backend/services/mqtt.py`:**

```python
# New topic subscriptions in _on_connect:
client.subscribe("heatpump/+/responses")
client.subscribe("heatpump/+/control/state")

# New method for publishing commands:
def publish_command(self, device_id: str, command_id: str,
                    command: str, params: dict) -> bool:
    """Publish command to device via MQTT."""
    topic = f"heatpump/{device_id}/commands"
    payload = {
        "command_id": command_id,
        "command": command,
        "params": params,
        "timestamp": int(datetime.now(timezone.utc).timestamp())
    }
    return self.publish(topic, payload)

# New handler for responses:
def _handle_command_response(self, device_id: str, data: dict) -> None:
    """Process command response from device."""
    command_id = data.get("command_id")
    result = data.get("result")

    # Update command status in database
    asyncio.run_coroutine_threadsafe(
        command_service.update_command_status(
            command_id=command_id,
            status="executed" if result == "success" else "failed",
            response_code=data.get("error_code"),
            response_message=data.get("error_message")
        ),
        self._loop
    )

    # Broadcast to WebSocket clients
    asyncio.run_coroutine_threadsafe(
        websocket_manager.broadcast_command_status(command_id, result, device_id),
        self._loop
    )
```

### 6.4 Configuration Additions

**Add to `server/backend/core/config.py`:**

```python
# PostgreSQL
DATABASE_URL: str = "postgresql://heatpump:password@localhost:5432/heatpump"

# JWT Authentication
JWT_SECRET_KEY: str = "your-secret-key-change-in-production"
JWT_ALGORITHM: str = "HS256"
JWT_ACCESS_TOKEN_EXPIRE_MINUTES: int = 30
JWT_REFRESH_TOKEN_EXPIRE_DAYS: int = 7

# Command Settings
COMMAND_TIMEOUT_SECONDS: int = 30
COMMAND_RATE_LIMIT_PER_MINUTE: int = 10
COMMAND_MAX_RETRIES: int = 3

# MQTT Topics (Stage 2)
MQTT_TOPIC_COMMANDS: str = "heatpump/{device_id}/commands"
MQTT_TOPIC_RESPONSES: str = "heatpump/+/responses"
MQTT_TOPIC_CONTROL_STATE: str = "heatpump/+/control/state"
```

### 6.5 Dependencies

**Add to `server/backend/requirements.txt`:**

```
# Database
asyncpg==0.29.0
sqlalchemy[asyncio]==2.0.25
alembic==1.13.1

# Authentication
python-jose[cryptography]==3.3.0
passlib[bcrypt]==1.7.4

# Rate Limiting
slowapi==0.1.9
```

---

## 7. Mobile App Implementation

### 7.1 Framework: Flutter

| Factor | Recommendation |
|--------|----------------|
| Framework | **Flutter** (Dart) |
| State Management | Riverpod 2.0 |
| Local Storage | Hive (for offline queue) |
| HTTP Client | Dio |
| WebSocket | web_socket_channel |
| Charts | fl_chart |
| UI Components | Material 3 |

### 7.2 Project Structure

```
mobile_app/
├── lib/
│   ├── main.dart
│   │
│   ├── core/
│   │   ├── config/
│   │   │   ├── app_config.dart         # API URLs, timeouts
│   │   │   └── theme_config.dart
│   │   ├── constants/
│   │   │   └── colors.dart
│   │   └── utils/
│   │       ├── validators.dart
│   │       └── formatters.dart
│   │
│   ├── data/
│   │   ├── models/
│   │   │   ├── device.dart
│   │   │   ├── sensor_data.dart
│   │   │   ├── command.dart
│   │   │   ├── control_state.dart
│   │   │   └── user.dart
│   │   ├── repositories/
│   │   │   ├── auth_repository.dart
│   │   │   ├── device_repository.dart
│   │   │   └── command_repository.dart
│   │   └── datasources/
│   │       ├── api_client.dart
│   │       ├── websocket_client.dart
│   │       └── local_storage.dart
│   │
│   ├── domain/
│   │   └── usecases/
│   │       ├── login_usecase.dart
│   │       ├── send_command_usecase.dart
│   │       └── get_device_status_usecase.dart
│   │
│   ├── presentation/
│   │   ├── screens/
│   │   │   ├── splash/
│   │   │   ├── auth/
│   │   │   │   ├── login_screen.dart
│   │   │   │   └── register_screen.dart
│   │   │   ├── home/
│   │   │   │   ├── home_screen.dart
│   │   │   │   └── widgets/
│   │   │   │       ├── device_card.dart
│   │   │   │       └── status_badge.dart
│   │   │   ├── device_detail/
│   │   │   │   ├── device_detail_screen.dart
│   │   │   │   └── tabs/
│   │   │   │       ├── overview_tab.dart
│   │   │   │       ├── control_tab.dart
│   │   │   │       ├── charts_tab.dart
│   │   │   │       └── alerts_tab.dart
│   │   │   └── settings/
│   │   │       └── settings_screen.dart
│   │   │
│   │   └── widgets/
│   │       ├── common/
│   │       │   ├── loading_overlay.dart
│   │       │   ├── error_dialog.dart
│   │       │   └── confirmation_dialog.dart
│   │       └── controls/
│   │           ├── mode_selector.dart
│   │           ├── fan_speed_control.dart
│   │           ├── defrost_button.dart
│   │           └── emergency_stop_button.dart
│   │
│   ├── providers/
│   │   ├── auth_provider.dart
│   │   ├── devices_provider.dart
│   │   ├── websocket_provider.dart
│   │   └── command_provider.dart
│   │
│   └── services/
│       ├── notification_service.dart
│       └── offline_queue_service.dart
│
├── pubspec.yaml
└── README.md
```

### 7.3 Key Screens

**Control Tab Wireframe:**

```
┌────────────────────────────────────────┐
│  ← Back       Site 1           ⚙️      │
├────────────────────────────────────────┤
│                                        │
│  STATUS:  ● ONLINE    MODE: HEATING    │
│                                        │
│  ╔════════════════════════════════════╗│
│  ║           MODE SELECTOR            ║│
│  ║                                    ║│
│  ║  ┌─────┐ ┌─────┐ ┌─────┐ ┌─────┐  ║│
│  ║  │ OFF │ │COOL │ │HEAT │ │ FAN │  ║│
│  ║  └─────┘ └─────┘ └▀▀▀▀▀┘ └─────┘  ║│
│  ║                    ▲               ║│
│  ╚════════════════════════════════════╝│
│                                        │
│  ╔════════════════════════════════════╗│
│  ║           FAN SPEED                ║│
│  ║                                    ║│
│  ║  ┌─────┐ ┌─────┐ ┌─────┐ ┌─────┐  ║│
│  ║  │ LOW │ │ MED │ │HIGH │ │AUTO │  ║│
│  ║  └─────┘ └▀▀▀▀▀┘ └─────┘ └─────┘  ║│
│  ║             ▲                      ║│
│  ╚════════════════════════════════════╝│
│                                        │
│  ╔════════════════════════════════════╗│
│  ║         QUICK ACTIONS              ║│
│  ║                                    ║│
│  ║    ┌──────────┐   ┌──────────┐    ║│
│  ║    │ DEFROST  │   │  E-STOP  │    ║│
│  ║    │    ❄️    │   │    🛑    │    ║│
│  ║    └──────────┘   └──────────┘    ║│
│  ╚════════════════════════════════════╝│
│                                        │
│  ┌────────────────────────────────────┐│
│  │ Current State                      ││
│  │ • Compressor: Running              ││
│  │ • Fan: Medium                      ││
│  │ • Defrost: Off                     ││
│  │ • Last command: 2 min ago ✓        ││
│  └────────────────────────────────────┘│
│                                        │
├────────────────────────────────────────┤
│  Overview    Control    Charts    📊   │
└────────────────────────────────────────┘
```

### 7.4 Offline Command Queuing

```dart
class OfflineQueueService {
  final Box<CommandQueueItem> _queueBox;
  final ConnectivityService _connectivity;
  final ApiClient _api;

  // Queue command when offline
  Future<void> queueCommand(String deviceId, String command,
                            Map<String, dynamic> params) async {
    final item = CommandQueueItem(
      id: const Uuid().v4(),
      deviceId: deviceId,
      command: command,
      params: params,
      createdAt: DateTime.now(),
      status: QueueStatus.pending,
    );

    await _queueBox.put(item.id, item);

    // Try to send immediately if connected
    if (await _connectivity.isConnected) {
      await _processQueue();
    }
  }

  // Process queued commands when back online
  Future<void> _processQueue() async {
    final pendingItems = _queueBox.values
        .where((item) => item.status == QueueStatus.pending)
        .toList()
      ..sort((a, b) => a.createdAt.compareTo(b.createdAt));

    for (final item in pendingItems) {
      try {
        final response = await _api.sendCommand(
          item.deviceId, item.command, item.params
        );
        item.status = QueueStatus.sent;
        item.serverCommandId = response.commandId;
        await _queueBox.put(item.id, item);
      } catch (e) {
        item.retryCount++;
        if (item.retryCount >= 3) {
          item.status = QueueStatus.failed;
        }
        await _queueBox.put(item.id, item);
        break; // Stop processing on failure
      }
    }
  }
}
```

### 7.5 Dependencies (`pubspec.yaml`)

```yaml
dependencies:
  flutter:
    sdk: flutter

  # State Management
  flutter_riverpod: ^2.4.9
  riverpod_annotation: ^2.3.3

  # Network
  dio: ^5.4.0
  web_socket_channel: ^2.4.0
  connectivity_plus: ^5.0.2

  # Local Storage
  hive: ^2.2.3
  hive_flutter: ^1.1.0

  # Authentication
  flutter_secure_storage: ^9.0.0
  jwt_decoder: ^2.0.1

  # UI
  flutter_svg: ^2.0.9
  fl_chart: ^0.65.0
  shimmer: ^3.0.0

  # Utilities
  uuid: ^4.2.2
  intl: ^0.18.1
  logger: ^2.0.2+1

dev_dependencies:
  flutter_test:
    sdk: flutter
  build_runner: ^2.4.8
  riverpod_generator: ^2.3.9
  hive_generator: ^2.0.1
  flutter_lints: ^3.0.1
```

---

## 8. Security Design

### 8.1 Authentication Flow

```
┌────────────┐         ┌────────────┐         ┌────────────┐
│ Mobile App │         │  Backend   │         │  Database  │
└─────┬──────┘         └─────┬──────┘         └─────┬──────┘
      │                      │                      │
      │  POST /auth/login    │                      │
      │  {email, password}   │                      │
      │─────────────────────>│                      │
      │                      │                      │
      │                      │  Verify credentials  │
      │                      │─────────────────────>│
      │                      │<─────────────────────│
      │                      │                      │
      │  {access_token,      │                      │
      │   refresh_token}     │                      │
      │<─────────────────────│                      │
      │                      │                      │
      │  GET /devices        │                      │
      │  Authorization:      │                      │
      │  Bearer <token>      │                      │
      │─────────────────────>│                      │
      │                      │  Validate JWT        │
      │                      │  Check permissions   │
      │                      │─────────────────────>│
      │                      │<─────────────────────│
      │                      │                      │
      │  [devices]           │                      │
      │<─────────────────────│                      │
```

### 8.2 Authorization Model

| Permission | View Sensors | Send Commands | Manage Users |
|------------|--------------|---------------|--------------|
| `can_view` | ✓ | ✗ | ✗ |
| `can_control` | ✓ | ✓ | ✗ |
| `can_admin` | ✓ | ✓ | ✓ |

### 8.3 Rate Limiting

| Endpoint | Limit | Window |
|----------|-------|--------|
| `/auth/login` | 5 attempts | 15 minutes |
| `/devices/{id}/command` | 10 commands | 1 minute |
| All other endpoints | 100 requests | 1 minute |

### 8.4 MQTT Security

1. **TLS encryption** - All MQTT traffic over port 8883
2. **Device authentication** - Unique username/password per device
3. **Topic ACLs** - Devices can only publish/subscribe to their own topics

```
# Mosquitto ACL configuration
user device_site1
topic write heatpump/site1/data
topic write heatpump/site1/alerts
topic write heatpump/site1/responses
topic write heatpump/site1/control/state
topic read heatpump/site1/commands

user backend
topic read heatpump/+/data
topic read heatpump/+/alerts
topic read heatpump/+/responses
topic read heatpump/+/control/state
topic write heatpump/+/commands
```

### 8.5 Audit Logging

All commands are logged with:
- User ID and IP address
- Device ID
- Command type and parameters
- Timestamp
- Result (success/failure)

---

## 9. Testing & Verification

### 9.1 Firmware Testing

| Test | Method | Pass Criteria |
|------|--------|---------------|
| Relay Toggle | Serial command `RELAY 1 ON` | Relay clicks, LED on |
| Interlock - Restart Delay | Send `set_mode heating` twice within 3 min | Second command rejected |
| Interlock - Pressure | Simulate high pressure, send `set_mode cooling` | Command rejected |
| Watchdog | Comment out `feedWatchdog()`, wait 200ms | ESP32 resets |
| E-Stop | Send `emergency_stop activate` | All relays de-energize |
| MQTT Command | Publish to commands topic | Response on responses topic |

### 9.2 Backend Testing

```bash
# Health check
curl http://localhost:8000/api/health

# Login
TOKEN=$(curl -X POST http://localhost:8000/api/auth/login \
  -H "Content-Type: application/json" \
  -d '{"email":"test@example.com","password":"password"}' \
  | jq -r '.access_token')

# Send command
curl -X POST http://localhost:8000/api/devices/site1/command \
  -H "Authorization: Bearer $TOKEN" \
  -H "Content-Type: application/json" \
  -d '{"command":"set_mode","params":{"mode":"heating"}}'

# Check command status
curl http://localhost:8000/api/devices/site1/command/{command_id} \
  -H "Authorization: Bearer $TOKEN"
```

### 9.3 End-to-End Test

1. Open mobile app, login
2. Navigate to device control
3. Select "Heating" mode
4. Observe:
   - Command sent (loading indicator)
   - Response received (success/failure)
   - Control state updates
   - Physical relay clicks (if connected to hardware)

### 9.4 Safety Test Checklist

- [ ] Compressor cannot start within 3 minutes of stopping
- [ ] Fan starts before compressor
- [ ] Fan continues after compressor stops
- [ ] High pressure triggers compressor cutout
- [ ] Low pressure triggers compressor cutout
- [ ] High temperature triggers compressor cutout
- [ ] Voltage out of range disables all outputs
- [ ] E-Stop de-energizes all relays
- [ ] Power failure leaves all outputs in safe state
- [ ] Watchdog resets ESP32 if firmware hangs

---

## 10. Implementation Timeline

### Phase 1: Hardware Setup (Weeks 1-2)
- [ ] Order components (relay module, contactor, watchdog, enclosure)
- [ ] Assemble relay board with protection components
- [ ] Wire to ESP32 development board
- [ ] Add pin definitions to `config.h`
- [ ] Implement basic `control.cpp` (relay on/off)
- [ ] Test with LEDs before connecting to actual loads
- **Deliverable:** Relays can be toggled via Serial commands

### Phase 2: Firmware Control Logic (Weeks 3-4)
- [ ] Implement `interlocks.cpp` safety logic
- [ ] Implement `command_handler.cpp` MQTT parsing
- [ ] Add MQTT subscription to commands topic
- [ ] Implement response publishing
- [ ] Implement control state publishing (retained)
- [ ] Watchdog feeding in main loop
- **Deliverable:** Device responds to MQTT commands with safety interlocks

### Phase 3: Backend Command Infrastructure (Weeks 5-6)
- [ ] Set up PostgreSQL database
- [ ] Create database migrations
- [ ] Implement JWT authentication
- [ ] Create command API endpoints
- [ ] Extend MQTT service for command publishing
- [ ] Add WebSocket command status broadcasting
- [ ] Implement rate limiting
- **Deliverable:** API accepts commands, publishes to MQTT, tracks status

### Phase 4: Mobile App MVP (Weeks 7-10)
- [ ] Flutter project setup with Riverpod
- [ ] Authentication screens (login/register)
- [ ] Home screen with device list
- [ ] Device detail with tabs (Overview, Control, Charts)
- [ ] Control panel widgets (mode selector, fan speed, e-stop)
- [ ] WebSocket integration for real-time updates
- [ ] Offline command queuing with Hive
- [ ] iOS and Android builds
- **Deliverable:** Working mobile app with control capability

### Phase 5: Integration & Field Testing (Weeks 11-12)
- [ ] Connect to actual heat pump unit
- [ ] Test under real operating conditions
- [ ] Verify all safety interlocks with actual sensors
- [ ] Load testing (relay switching under load)
- [ ] Fix bugs and edge cases
- [ ] UX improvements based on testing
- **Deliverable:** System ready for production deployment

### Phase 6: Production Deployment (Week 13+)
- [ ] Production server setup
- [ ] TLS certificates for MQTT broker
- [ ] App store submissions (iOS App Store, Google Play)
- [ ] User documentation
- [ ] Monitoring and alerting setup
- **Deliverable:** Live production system

---

## Appendix A: Bill of Materials (Complete)

| Item | Description | Qty | Unit Price | Total |
|------|-------------|-----|------------|-------|
| 8-Ch Relay Module | SainSmart, optocoupler isolated | 1 | ₹550 | ₹550 |
| Contactor | Schneider LC1D09, 24VAC coil | 1 | ₹900 | ₹900 |
| Watchdog Timer | TPL5010 breakout | 1 | ₹350 | ₹350 |
| HLK-PM01 | 5V 600mA AC-DC converter | 1 | ₹180 | ₹180 |
| 1N4007 Diodes | Flyback protection | 10 | ₹5 | ₹50 |
| 1K Resistors | 1/4W, for relay inputs | 10 | ₹2 | ₹20 |
| 10A Fuses | Glass tube, 250V | 10 | ₹10 | ₹100 |
| Fuse Holders | Panel mount | 8 | ₹20 | ₹160 |
| Terminal Blocks | 5.08mm, 2-pos | 10 | ₹20 | ₹200 |
| DIN Rail | 35mm, 200mm length | 1 | ₹100 | ₹100 |
| Enclosure | IP65, 200x150x100mm | 1 | ₹500 | ₹500 |
| Cable Glands | PG9/PG11 | 6 | ₹20 | ₹120 |
| Jumper Wires | 22AWG assorted | 1 set | ₹150 | ₹150 |
| Capacitors | 100uF, 470uF electrolytic | 4 | ₹10 | ₹40 |
| TVS Diodes | P6KE18CA | 4 | ₹15 | ₹60 |
| Varistor | 14D471K | 2 | ₹15 | ₹30 |
| Miscellaneous | Solder, heatshrink, screws | - | - | ₹200 |
| **TOTAL** | | | | **₹3,710** |

---

## Appendix B: Supplier Links (India)

| Component | Supplier | Link |
|-----------|----------|------|
| Relay Module | Robu | https://robu.in/product/8-channel-5v-relay-module/ |
| ESP32 DevKit | Robu | https://robu.in/product/esp32-development-board/ |
| Contactor | Amazon | Search "Schneider LC1D09" |
| TPL5010 | Evelta | https://evelta.com |
| Enclosure | Amazon | Search "IP65 junction box" |
| Generic Components | Electronicspices | https://electronicspices.com |

---

*Document Version: 1.0*
*Created: January 2026*
*Project: Smart Heat Pump Stage 2 - Control System*
