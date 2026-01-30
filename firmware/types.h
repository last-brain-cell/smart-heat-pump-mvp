/*
 * types.h - Data structures and enumerations
 */

#ifndef TYPES_H
#define TYPES_H

#include <Arduino.h>

// ============================================================================
// ALERT LEVEL ENUMERATION
// ============================================================================
enum AlertLevel {
    ALERT_OK = 0,
    ALERT_WARNING = 1,
    ALERT_CRITICAL = 2
};

// ============================================================================
// ALERT TYPE ENUMERATION
// Used for cooldown tracking - each alert type has its own cooldown
// ============================================================================
enum AlertType {
    ALERT_VOLTAGE_HIGH = 0,
    ALERT_VOLTAGE_LOW,
    ALERT_COMPRESSOR_TEMP,
    ALERT_PRESSURE_HIGH,
    ALERT_PRESSURE_LOW,
    ALERT_OVERCURRENT,
    ALERT_TYPE_COUNT  // Must be last - used for array sizing
};

// ============================================================================
// SMS COMMAND ENUMERATION
// ============================================================================
enum SMSCommand {
    SMS_CMD_NONE = 0,
    SMS_CMD_STATUS,
    SMS_CMD_RESET,
    SMS_CMD_UNKNOWN
};

// ============================================================================
// GSM STATE ENUMERATION
// ============================================================================
enum GSMState {
    GSM_UNINITIALIZED = 0,
    GSM_INITIALIZING,
    GSM_READY,
    GSM_CONNECTING_GPRS,
    GSM_GPRS_CONNECTED,
    GSM_ERROR
};

// ============================================================================
// INDIVIDUAL SENSOR READING
// ============================================================================
struct SensorReading {
    float value;
    AlertLevel alertLevel;
    unsigned long timestamp;
    bool valid;  // For handling sensor errors

    // Default constructor
    SensorReading() : value(0), alertLevel(ALERT_OK), timestamp(0), valid(false) {}
};

// ============================================================================
// COMPLETE SYSTEM DATA SNAPSHOT
// ============================================================================
struct SystemData {
    // Temperatures (Celsius)
    SensorReading tempInlet;
    SensorReading tempOutlet;
    SensorReading tempAmbient;
    SensorReading tempCompressor;

    // Electrical
    SensorReading voltage;      // Volts AC
    SensorReading current;      // Amps
    float power;                // Watts (calculated: voltage * current)

    // Pressure (PSI) - Optional
    SensorReading pressureHigh;
    SensorReading pressureLow;

    // System status flags
    bool compressorRunning;
    bool fanRunning;
    bool defrostActive;

    // Timestamp
    unsigned long readingTime;

    // Default constructor
    SystemData() : power(0), compressorRunning(false), fanRunning(false),
                   defrostActive(false), readingTime(0) {}
};

// ============================================================================
// ALERT COOLDOWN TRACKING
// ============================================================================
struct AlertCooldown {
    unsigned long lastAlertTime[ALERT_TYPE_COUNT];
    bool alertActive[ALERT_TYPE_COUNT];

    // Default constructor - initialize all to 0/false
    AlertCooldown() {
        for (int i = 0; i < ALERT_TYPE_COUNT; i++) {
            lastAlertTime[i] = 0;
            alertActive[i] = false;
        }
    }
};

// ============================================================================
// SMS MESSAGE STRUCTURE
// ============================================================================
struct SMSMessage {
    String sender;
    String content;
    bool isNew;

    SMSMessage() : isNew(false) {}
};

// ============================================================================
// HELPER FUNCTIONS
// ============================================================================

// Get alert type name as string
inline const char* getAlertTypeName(AlertType type) {
    switch (type) {
        case ALERT_VOLTAGE_HIGH:    return "HIGH VOLTAGE";
        case ALERT_VOLTAGE_LOW:     return "LOW VOLTAGE";
        case ALERT_COMPRESSOR_TEMP: return "COMPRESSOR TEMP";
        case ALERT_PRESSURE_HIGH:   return "HIGH PRESSURE";
        case ALERT_PRESSURE_LOW:    return "LOW PRESSURE";
        case ALERT_OVERCURRENT:     return "OVERCURRENT";
        default:                    return "UNKNOWN";
    }
}

// Get alert level name as string
inline const char* getAlertLevelName(AlertLevel level) {
    switch (level) {
        case ALERT_OK:       return "OK";
        case ALERT_WARNING:  return "WARNING";
        case ALERT_CRITICAL: return "CRITICAL";
        default:             return "UNKNOWN";
    }
}

#endif // TYPES_H
