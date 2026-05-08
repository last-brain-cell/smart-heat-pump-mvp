/**
 * @file types.h
 * @brief V2 data structures and enumerations
 *
 * Defines all shared data types for the 3-phase monitoring firmware.
 * Key differences from v1:
 *   - PhaseData struct for per-phase PZEM-004T readings
 *   - SystemData has 2 temps (DS18B20) instead of 4 (NTC)
 *   - Per-phase alert types (9 total instead of 6)
 *   - No pressure sensor support
 */

#ifndef TYPES_H
#define TYPES_H

#include <Arduino.h>

// =============================================================================
// ENUMERATIONS
// =============================================================================

enum AlertLevel {
    ALERT_OK = 0,
    ALERT_WARNING = 1,
    ALERT_CRITICAL = 2
};

/**
 * @brief Per-phase alert types for cooldown tracking
 * 3 phases x (voltage_high + voltage_low + overcurrent) = 9 types
 */
enum AlertType {
    ALERT_VOLTAGE_HIGH_P1 = 0,
    ALERT_VOLTAGE_LOW_P1,
    ALERT_VOLTAGE_HIGH_P2,
    ALERT_VOLTAGE_LOW_P2,
    ALERT_VOLTAGE_HIGH_P3,
    ALERT_VOLTAGE_LOW_P3,
    ALERT_OVERCURRENT_P1,
    ALERT_OVERCURRENT_P2,
    ALERT_OVERCURRENT_P3,
    ALERT_TYPE_COUNT  ///< Must be last
};

enum SMSCommand {
    SMS_CMD_NONE = 0,
    SMS_CMD_STATUS,
    SMS_CMD_RESET,
    SMS_CMD_WIFI_RESET,
    SMS_CMD_UNKNOWN
};

enum GSMState {
    GSM_UNINITIALIZED = 0,
    GSM_INITIALIZING,
    GSM_READY,
    GSM_CONNECTING_GPRS,
    GSM_GPRS_CONNECTED,
    GSM_STATE_ERROR
};

enum ConnectionType {
    CONN_NONE = 0,
    CONN_WIFI,
    CONN_GPRS
};

// =============================================================================
// DATA STRUCTURES
// =============================================================================

struct SensorReading {
    float value;
    AlertLevel alertLevel;
    unsigned long timestamp;
    bool valid;

    SensorReading() : value(0), alertLevel(ALERT_OK), timestamp(0), valid(false) {}
};

/**
 * @brief Single phase electrical data from one PZEM-004T v3.0
 *
 * The PZEM module provides all these values directly — no calculation needed.
 */
struct PhaseData {
    SensorReading voltage;   ///< Volts AC (80-260V range)
    SensorReading current;   ///< Amps (0-100A range)
    float power;             ///< Watts (from PZEM, not calculated)
    float energy;            ///< kWh cumulative
    float frequency;         ///< Hz (45-65Hz range)
    float powerFactor;       ///< 0.00 - 1.00
    bool valid;              ///< PZEM communication OK

    PhaseData() : power(0), energy(0), frequency(0), powerFactor(0), valid(false) {}
};

/**
 * @brief Complete V2 system data snapshot
 *
 * V2 differences from V1:
 *   - 2 temperature sensors (DS18B20) instead of 4 (NTC)
 *   - 3 phases of electrical data instead of 1
 *   - No pressure sensors
 */
struct SystemData {
    // Temperatures (DS18B20 via OneWire)
    SensorReading tempInlet;
    SensorReading tempOutlet;

    // 3-phase electrical (PZEM-004T via Modbus)
    PhaseData phase[3];  ///< phase[0]=P1, phase[1]=P2, phase[2]=P3

    // System status flags
    bool compressorRunning;  ///< Detected from total current draw > 1A
    bool fanRunning;
    bool defrostActive;

    unsigned long readingTime;

    SystemData() : compressorRunning(false), fanRunning(false),
                   defrostActive(false), readingTime(0) {}
};

struct AlertCooldown {
    unsigned long lastAlertTime[ALERT_TYPE_COUNT];
    bool alertActive[ALERT_TYPE_COUNT];

    AlertCooldown() {
        for (int i = 0; i < ALERT_TYPE_COUNT; i++) {
            lastAlertTime[i] = 0;
            alertActive[i] = false;
        }
    }
};

struct SMSMessage {
    String sender;
    String content;
    bool isNew;

    SMSMessage() : isNew(false) {}
};

// =============================================================================
// HELPER FUNCTIONS
// =============================================================================

inline const char* getAlertTypeName(AlertType type) {
    switch (type) {
        case ALERT_VOLTAGE_HIGH_P1:  return "HIGH VOLTAGE P1";
        case ALERT_VOLTAGE_LOW_P1:   return "LOW VOLTAGE P1";
        case ALERT_VOLTAGE_HIGH_P2:  return "HIGH VOLTAGE P2";
        case ALERT_VOLTAGE_LOW_P2:   return "LOW VOLTAGE P2";
        case ALERT_VOLTAGE_HIGH_P3:  return "HIGH VOLTAGE P3";
        case ALERT_VOLTAGE_LOW_P3:   return "LOW VOLTAGE P3";
        case ALERT_OVERCURRENT_P1:   return "OVERCURRENT P1";
        case ALERT_OVERCURRENT_P2:   return "OVERCURRENT P2";
        case ALERT_OVERCURRENT_P3:   return "OVERCURRENT P3";
        default:                     return "UNKNOWN";
    }
}

inline const char* getAlertLevelName(AlertLevel level) {
    switch (level) {
        case ALERT_OK:       return "OK";
        case ALERT_WARNING:  return "WARNING";
        case ALERT_CRITICAL: return "CRITICAL";
        default:             return "UNKNOWN";
    }
}

#endif // TYPES_H
