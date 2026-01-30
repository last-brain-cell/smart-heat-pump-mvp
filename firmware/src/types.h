/**
 * @file types.h
 * @brief Data structures and enumerations
 *
 * Defines all shared data types used across the firmware modules.
 */

#ifndef TYPES_H
#define TYPES_H

#include <Arduino.h>

// =============================================================================
// ENUMERATIONS
// =============================================================================

/**
 * @brief Alert severity levels
 */
enum AlertLevel {
    ALERT_OK = 0,       ///< Normal operating range
    ALERT_WARNING = 1,  ///< Approaching threshold
    ALERT_CRITICAL = 2  ///< Threshold exceeded
};

/**
 * @brief Alert types for cooldown tracking
 */
enum AlertType {
    ALERT_VOLTAGE_HIGH = 0,
    ALERT_VOLTAGE_LOW,
    ALERT_COMPRESSOR_TEMP,
    ALERT_PRESSURE_HIGH,
    ALERT_PRESSURE_LOW,
    ALERT_OVERCURRENT,
    ALERT_TYPE_COUNT  ///< Must be last - used for array sizing
};

/**
 * @brief SMS command types
 */
enum SMSCommand {
    SMS_CMD_NONE = 0,
    SMS_CMD_STATUS,
    SMS_CMD_RESET,
    SMS_CMD_UNKNOWN
};

/**
 * @brief GSM module state machine
 */
enum GSMState {
    GSM_UNINITIALIZED = 0,
    GSM_INITIALIZING,
    GSM_READY,
    GSM_CONNECTING_GPRS,
    GSM_GPRS_CONNECTED,
    GSM_ERROR
};

// =============================================================================
// DATA STRUCTURES
// =============================================================================

/**
 * @brief Individual sensor reading with metadata
 */
struct SensorReading {
    float value;              ///< Measured value
    AlertLevel alertLevel;    ///< Current alert state
    unsigned long timestamp;  ///< Reading time (millis)
    bool valid;               ///< Validity flag

    SensorReading() : value(0), alertLevel(ALERT_OK), timestamp(0), valid(false) {}
};

/**
 * @brief Complete system data snapshot
 */
struct SystemData {
    // Temperatures (Celsius)
    SensorReading tempInlet;
    SensorReading tempOutlet;
    SensorReading tempAmbient;
    SensorReading tempCompressor;

    // Electrical
    SensorReading voltage;   ///< Volts AC
    SensorReading current;   ///< Amps
    float power;             ///< Watts (calculated)

    // Pressure (PSI) - Optional sensors
    SensorReading pressureHigh;
    SensorReading pressureLow;

    // System status flags
    bool compressorRunning;
    bool fanRunning;
    bool defrostActive;

    // Timestamp
    unsigned long readingTime;

    SystemData() : power(0), compressorRunning(false), fanRunning(false),
                   defrostActive(false), readingTime(0) {}
};

/**
 * @brief Alert cooldown tracking state
 */
struct AlertCooldown {
    unsigned long lastAlertTime[ALERT_TYPE_COUNT];  ///< Last alert timestamp per type
    bool alertActive[ALERT_TYPE_COUNT];             ///< Active alert flag per type

    AlertCooldown() {
        for (int i = 0; i < ALERT_TYPE_COUNT; i++) {
            lastAlertTime[i] = 0;
            alertActive[i] = false;
        }
    }
};

/**
 * @brief SMS message data
 */
struct SMSMessage {
    String sender;   ///< Sender phone number
    String content;  ///< Message text
    bool isNew;      ///< Unread flag

    SMSMessage() : isNew(false) {}
};

// =============================================================================
// HELPER FUNCTIONS
// =============================================================================

/**
 * @brief Get human-readable alert type name
 * @param type Alert type
 * @return Static string with type name
 */
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

/**
 * @brief Get human-readable alert level name
 * @param level Alert level
 * @return Static string with level name
 */
inline const char* getAlertLevelName(AlertLevel level) {
    switch (level) {
        case ALERT_OK:       return "OK";
        case ALERT_WARNING:  return "WARNING";
        case ALERT_CRITICAL: return "CRITICAL";
        default:             return "UNKNOWN";
    }
}

#endif // TYPES_H
