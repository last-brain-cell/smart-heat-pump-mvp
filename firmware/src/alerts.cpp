/**
 * @file alerts.cpp
 * @brief Alert management implementation
 */

#include "alerts.h"
#include "globals.h"
#include "gsm.h"

// =============================================================================
// PRIVATE DATA
// =============================================================================

static AlertCooldown alertCooldowns;

// =============================================================================
// IMPLEMENTATION
// =============================================================================

void initAlerts() {
    for (int i = 0; i < ALERT_TYPE_COUNT; i++) {
        alertCooldowns.lastAlertTime[i] = 0;
        alertCooldowns.alertActive[i] = false;
    }
    Log.println(F("[ALERTS] Initialized"));
}

AlertLevel checkVoltage(float voltage, bool* isHigh) {
    // Check high voltage
    if (voltage >= VOLTAGE_HIGH_CRITICAL) {
        *isHigh = true;
        return ALERT_CRITICAL;
    }
    if (voltage >= VOLTAGE_HIGH_WARNING) {
        *isHigh = true;
        return ALERT_WARNING;
    }

    // Check low voltage
    if (voltage <= VOLTAGE_LOW_CRITICAL) {
        *isHigh = false;
        return ALERT_CRITICAL;
    }
    if (voltage <= VOLTAGE_LOW_WARNING) {
        *isHigh = false;
        return ALERT_WARNING;
    }

    return ALERT_OK;
}

AlertLevel checkCompressorTemp(float temp) {
    if (temp >= COMP_TEMP_CRITICAL) {
        return ALERT_CRITICAL;
    }
    if (temp >= COMP_TEMP_WARNING) {
        return ALERT_WARNING;
    }
    return ALERT_OK;
}

AlertLevel checkPressureHigh(float pressure) {
    if (pressure >= PRESSURE_HIGH_CRITICAL) {
        return ALERT_CRITICAL;
    }
    if (pressure >= PRESSURE_HIGH_WARNING) {
        return ALERT_WARNING;
    }
    return ALERT_OK;
}

AlertLevel checkPressureLow(float pressure) {
    if (pressure <= PRESSURE_LOW_CRITICAL) {
        return ALERT_CRITICAL;
    }
    if (pressure <= PRESSURE_LOW_WARNING) {
        return ALERT_WARNING;
    }
    return ALERT_OK;
}

AlertLevel checkCurrent(float current) {
    if (current >= CURRENT_CRITICAL) {
        return ALERT_CRITICAL;
    }
    if (current >= CURRENT_WARNING) {
        return ALERT_WARNING;
    }
    return ALERT_OK;
}

bool canSendAlert(AlertType type) {
    if (type >= ALERT_TYPE_COUNT) {
        return false;
    }

    unsigned long now = millis();
    unsigned long lastAlert = alertCooldowns.lastAlertTime[type];

    // Handle millis() overflow
    if (now < lastAlert) {
        alertCooldowns.lastAlertTime[type] = 0;
        return true;
    }

    return (now - lastAlert) >= ALERT_COOLDOWN;
}

void recordAlertSent(AlertType type) {
    if (type >= ALERT_TYPE_COUNT) {
        return;
    }

    alertCooldowns.lastAlertTime[type] = millis();
    alertCooldowns.alertActive[type] = true;

    Log.print(F("[ALERTS] Alert recorded: "));
    Log.println(getAlertTypeName(type));
}

void resetAlertCooldown(AlertType type) {
    if (type >= ALERT_TYPE_COUNT) {
        return;
    }

    if (alertCooldowns.alertActive[type]) {
        alertCooldowns.alertActive[type] = false;
        Log.print(F("[ALERTS] Alert cleared: "));
        Log.println(getAlertTypeName(type));
    }
}

size_t formatAlertMessage(AlertType type, AlertLevel level, float value,
                          char* buffer, size_t bufferSize) {
    const char* unit = "";
    int precision = 1;

    switch (type) {
        case ALERT_VOLTAGE_HIGH:
        case ALERT_VOLTAGE_LOW:
            unit = "V";
            break;
        case ALERT_COMPRESSOR_TEMP:
            unit = "C";
            break;
        case ALERT_PRESSURE_HIGH:
        case ALERT_PRESSURE_LOW:
            unit = "PSI";
            precision = 0;
            break;
        case ALERT_OVERCURRENT:
            unit = "A";
            break;
        default:
            break;
    }

    if (precision == 0) {
        return snprintf(buffer, bufferSize,
            "ALERT: %s\n"
            "Level: %s\n"
            "Value: %.0f %s\n\n"
            "Device: %s",
            getAlertTypeName(type),
            getAlertLevelName(level),
            value, unit,
            DEVICE_ID
        );
    }

    return snprintf(buffer, bufferSize,
        "ALERT: %s\n"
        "Level: %s\n"
        "Value: %.1f %s\n\n"
        "Device: %s",
        getAlertTypeName(type),
        getAlertLevelName(level),
        value, unit,
        DEVICE_ID
    );
}

void checkAllAlerts(SystemData& data) {
    bool isHighVoltage;
    char alertBuffer[SMS_BUFFER_SIZE];

    // ---- VOLTAGE CHECK ----
    AlertLevel voltageAlert = checkVoltage(data.voltage.value, &isHighVoltage);
    data.voltage.alertLevel = voltageAlert;

    if (voltageAlert != ALERT_OK) {
        AlertType alertType = isHighVoltage ? ALERT_VOLTAGE_HIGH : ALERT_VOLTAGE_LOW;

        if (voltageAlert == ALERT_CRITICAL && canSendAlert(alertType)) {
            formatAlertMessage(alertType, voltageAlert, data.voltage.value,
                               alertBuffer, sizeof(alertBuffer));
            if (sendSMS(ADMIN_PHONE, alertBuffer)) {
                recordAlertSent(alertType);
            }
        }
    } else {
        resetAlertCooldown(ALERT_VOLTAGE_HIGH);
        resetAlertCooldown(ALERT_VOLTAGE_LOW);
    }

    // ---- COMPRESSOR TEMPERATURE CHECK ----
    AlertLevel tempAlert = checkCompressorTemp(data.tempCompressor.value);
    data.tempCompressor.alertLevel = tempAlert;

    if (tempAlert == ALERT_CRITICAL && canSendAlert(ALERT_COMPRESSOR_TEMP)) {
        formatAlertMessage(ALERT_COMPRESSOR_TEMP, tempAlert, data.tempCompressor.value,
                           alertBuffer, sizeof(alertBuffer));
        if (sendSMS(ADMIN_PHONE, alertBuffer)) {
            recordAlertSent(ALERT_COMPRESSOR_TEMP);
        }
    } else if (tempAlert == ALERT_OK) {
        resetAlertCooldown(ALERT_COMPRESSOR_TEMP);
    }

    // ---- HIGH PRESSURE CHECK ----
    AlertLevel highPressureAlert = checkPressureHigh(data.pressureHigh.value);
    data.pressureHigh.alertLevel = highPressureAlert;

    if (highPressureAlert == ALERT_CRITICAL && canSendAlert(ALERT_PRESSURE_HIGH)) {
        formatAlertMessage(ALERT_PRESSURE_HIGH, highPressureAlert, data.pressureHigh.value,
                           alertBuffer, sizeof(alertBuffer));
        if (sendSMS(ADMIN_PHONE, alertBuffer)) {
            recordAlertSent(ALERT_PRESSURE_HIGH);
        }
    } else if (highPressureAlert == ALERT_OK) {
        resetAlertCooldown(ALERT_PRESSURE_HIGH);
    }

    // ---- LOW PRESSURE CHECK ----
    AlertLevel lowPressureAlert = checkPressureLow(data.pressureLow.value);
    data.pressureLow.alertLevel = lowPressureAlert;

    if (lowPressureAlert == ALERT_CRITICAL && canSendAlert(ALERT_PRESSURE_LOW)) {
        formatAlertMessage(ALERT_PRESSURE_LOW, lowPressureAlert, data.pressureLow.value,
                           alertBuffer, sizeof(alertBuffer));
        if (sendSMS(ADMIN_PHONE, alertBuffer)) {
            recordAlertSent(ALERT_PRESSURE_LOW);
        }
    } else if (lowPressureAlert == ALERT_OK) {
        resetAlertCooldown(ALERT_PRESSURE_LOW);
    }

    // ---- OVERCURRENT CHECK ----
    AlertLevel currentAlert = checkCurrent(data.current.value);
    data.current.alertLevel = currentAlert;

    if (currentAlert == ALERT_CRITICAL && canSendAlert(ALERT_OVERCURRENT)) {
        formatAlertMessage(ALERT_OVERCURRENT, currentAlert, data.current.value,
                           alertBuffer, sizeof(alertBuffer));
        if (sendSMS(ADMIN_PHONE, alertBuffer)) {
            recordAlertSent(ALERT_OVERCURRENT);
        }
    } else if (currentAlert == ALERT_OK) {
        resetAlertCooldown(ALERT_OVERCURRENT);
    }
}

size_t getAlertSummary(char* buffer, size_t bufferSize) {
    int activeCount = 0;
    size_t written = 0;

    for (int i = 0; i < ALERT_TYPE_COUNT; i++) {
        if (alertCooldowns.alertActive[i]) {
            activeCount++;
        }
    }

    if (activeCount == 0) {
        return snprintf(buffer, bufferSize, "No active alerts");
    }

    written = snprintf(buffer, bufferSize, "Active alerts: ");

    bool first = true;
    for (int i = 0; i < ALERT_TYPE_COUNT && written < bufferSize - 1; i++) {
        if (alertCooldowns.alertActive[i]) {
            if (!first && written < bufferSize - 2) {
                written += snprintf(buffer + written, bufferSize - written, ", ");
            }
            written += snprintf(buffer + written, bufferSize - written, "%s",
                                getAlertTypeName((AlertType)i));
            first = false;
        }
    }

    return written;
}
