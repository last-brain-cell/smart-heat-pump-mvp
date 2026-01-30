/*
 * alerts.h - Alert Management Module
 *
 * Handles threshold checking for all sensors and manages alert cooldowns
 * to prevent SMS spam.
 */

#ifndef ALERTS_H
#define ALERTS_H

#include <Arduino.h>
#include "config.h"
#include "types.h"

// ============================================================================
// GLOBAL ALERT COOLDOWN STATE
// ============================================================================
static AlertCooldown alertCooldowns;

// ============================================================================
// FUNCTION DECLARATIONS
// ============================================================================
void initAlerts();
AlertLevel checkVoltage(float voltage, bool* isHigh);
AlertLevel checkCompressorTemp(float temp);
AlertLevel checkPressureHigh(float pressure);
AlertLevel checkPressureLow(float pressure);
AlertLevel checkCurrent(float current);

bool canSendAlert(AlertType type);
void recordAlertSent(AlertType type);
void resetAlertCooldown(AlertType type);

String formatAlertMessage(AlertType type, AlertLevel level, float value);
void checkAllAlerts(SystemData& data);

// External function from gsm.h
extern bool sendSMS(const char* phone, const String& message);

// ============================================================================
// INITIALIZE ALERT SYSTEM
// ============================================================================
void initAlerts() {
    for (int i = 0; i < ALERT_TYPE_COUNT; i++) {
        alertCooldowns.lastAlertTime[i] = 0;
        alertCooldowns.alertActive[i] = false;
    }
    Serial.println(F("[ALERTS] Initialized"));
}

// ============================================================================
// CHECK VOLTAGE AGAINST THRESHOLDS
// Sets isHigh to true if voltage is high, false if low
// ============================================================================
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

// ============================================================================
// CHECK COMPRESSOR TEMPERATURE
// ============================================================================
AlertLevel checkCompressorTemp(float temp) {
    if (temp >= COMP_TEMP_CRITICAL) {
        return ALERT_CRITICAL;
    }
    if (temp >= COMP_TEMP_WARNING) {
        return ALERT_WARNING;
    }
    return ALERT_OK;
}

// ============================================================================
// CHECK HIGH SIDE PRESSURE
// ============================================================================
AlertLevel checkPressureHigh(float pressure) {
    if (pressure >= PRESSURE_HIGH_CRITICAL) {
        return ALERT_CRITICAL;
    }
    if (pressure >= PRESSURE_HIGH_WARNING) {
        return ALERT_WARNING;
    }
    return ALERT_OK;
}

// ============================================================================
// CHECK LOW SIDE PRESSURE
// ============================================================================
AlertLevel checkPressureLow(float pressure) {
    if (pressure <= PRESSURE_LOW_CRITICAL) {
        return ALERT_CRITICAL;
    }
    if (pressure <= PRESSURE_LOW_WARNING) {
        return ALERT_WARNING;
    }
    return ALERT_OK;
}

// ============================================================================
// CHECK CURRENT (OVERCURRENT)
// ============================================================================
AlertLevel checkCurrent(float current) {
    if (current >= CURRENT_CRITICAL) {
        return ALERT_CRITICAL;
    }
    if (current >= CURRENT_WARNING) {
        return ALERT_WARNING;
    }
    return ALERT_OK;
}

// ============================================================================
// CHECK IF ALERT CAN BE SENT (COOLDOWN MANAGEMENT)
// ============================================================================
bool canSendAlert(AlertType type) {
    if (type >= ALERT_TYPE_COUNT) return false;

    unsigned long now = millis();
    unsigned long lastAlert = alertCooldowns.lastAlertTime[type];

    // Handle millis() overflow
    if (now < lastAlert) {
        // Overflow occurred, reset cooldown
        alertCooldowns.lastAlertTime[type] = 0;
        return true;
    }

    return (now - lastAlert) >= ALERT_COOLDOWN;
}

// ============================================================================
// RECORD THAT AN ALERT WAS SENT
// ============================================================================
void recordAlertSent(AlertType type) {
    if (type >= ALERT_TYPE_COUNT) return;

    alertCooldowns.lastAlertTime[type] = millis();
    alertCooldowns.alertActive[type] = true;

    Serial.print(F("[ALERTS] Alert recorded: "));
    Serial.println(getAlertTypeName(type));
}

// ============================================================================
// RESET ALERT COOLDOWN (when condition clears)
// ============================================================================
void resetAlertCooldown(AlertType type) {
    if (type >= ALERT_TYPE_COUNT) return;

    if (alertCooldowns.alertActive[type]) {
        alertCooldowns.alertActive[type] = false;
        Serial.print(F("[ALERTS] Alert cleared: "));
        Serial.println(getAlertTypeName(type));
    }
}

// ============================================================================
// FORMAT ALERT MESSAGE FOR SMS
// ============================================================================
String formatAlertMessage(AlertType type, AlertLevel level, float value) {
    String msg = "ALERT: ";
    msg += getAlertTypeName(type);
    msg += "\n";

    msg += "Level: ";
    msg += getAlertLevelName(level);
    msg += "\n";

    msg += "Value: ";

    switch (type) {
        case ALERT_VOLTAGE_HIGH:
        case ALERT_VOLTAGE_LOW:
            msg += String(value, 1) + " V";
            break;
        case ALERT_COMPRESSOR_TEMP:
            msg += String(value, 1) + " C";
            break;
        case ALERT_PRESSURE_HIGH:
        case ALERT_PRESSURE_LOW:
            msg += String(value, 0) + " PSI";
            break;
        case ALERT_OVERCURRENT:
            msg += String(value, 1) + " A";
            break;
        default:
            msg += String(value, 1);
    }

    msg += "\n\nDevice: ";
    msg += DEVICE_ID;

    return msg;
}

// ============================================================================
// CHECK ALL SENSOR VALUES AND SEND ALERTS IF NEEDED
// Updates alertLevel field in SystemData structure
// ============================================================================
void checkAllAlerts(SystemData& data) {
    bool isHighVoltage;

    // ---- VOLTAGE CHECK ----
    AlertLevel voltageAlert = checkVoltage(data.voltage.value, &isHighVoltage);
    data.voltage.alertLevel = voltageAlert;

    if (voltageAlert != ALERT_OK) {
        AlertType alertType = isHighVoltage ? ALERT_VOLTAGE_HIGH : ALERT_VOLTAGE_LOW;

        // Send SMS only for CRITICAL alerts
        if (voltageAlert == ALERT_CRITICAL && canSendAlert(alertType)) {
            String msg = formatAlertMessage(alertType, voltageAlert, data.voltage.value);
            if (sendSMS(ADMIN_PHONE, msg)) {
                recordAlertSent(alertType);
            }
        }
    } else {
        // Voltage is OK - reset cooldowns for both high and low
        resetAlertCooldown(ALERT_VOLTAGE_HIGH);
        resetAlertCooldown(ALERT_VOLTAGE_LOW);
    }

    // ---- COMPRESSOR TEMPERATURE CHECK ----
    AlertLevel tempAlert = checkCompressorTemp(data.tempCompressor.value);
    data.tempCompressor.alertLevel = tempAlert;

    if (tempAlert == ALERT_CRITICAL && canSendAlert(ALERT_COMPRESSOR_TEMP)) {
        String msg = formatAlertMessage(ALERT_COMPRESSOR_TEMP, tempAlert, data.tempCompressor.value);
        if (sendSMS(ADMIN_PHONE, msg)) {
            recordAlertSent(ALERT_COMPRESSOR_TEMP);
        }
    } else if (tempAlert == ALERT_OK) {
        resetAlertCooldown(ALERT_COMPRESSOR_TEMP);
    }

    // ---- HIGH PRESSURE CHECK ----
    AlertLevel highPressureAlert = checkPressureHigh(data.pressureHigh.value);
    data.pressureHigh.alertLevel = highPressureAlert;

    if (highPressureAlert == ALERT_CRITICAL && canSendAlert(ALERT_PRESSURE_HIGH)) {
        String msg = formatAlertMessage(ALERT_PRESSURE_HIGH, highPressureAlert, data.pressureHigh.value);
        if (sendSMS(ADMIN_PHONE, msg)) {
            recordAlertSent(ALERT_PRESSURE_HIGH);
        }
    } else if (highPressureAlert == ALERT_OK) {
        resetAlertCooldown(ALERT_PRESSURE_HIGH);
    }

    // ---- LOW PRESSURE CHECK ----
    AlertLevel lowPressureAlert = checkPressureLow(data.pressureLow.value);
    data.pressureLow.alertLevel = lowPressureAlert;

    if (lowPressureAlert == ALERT_CRITICAL && canSendAlert(ALERT_PRESSURE_LOW)) {
        String msg = formatAlertMessage(ALERT_PRESSURE_LOW, lowPressureAlert, data.pressureLow.value);
        if (sendSMS(ADMIN_PHONE, msg)) {
            recordAlertSent(ALERT_PRESSURE_LOW);
        }
    } else if (lowPressureAlert == ALERT_OK) {
        resetAlertCooldown(ALERT_PRESSURE_LOW);
    }

    // ---- OVERCURRENT CHECK ----
    AlertLevel currentAlert = checkCurrent(data.current.value);
    data.current.alertLevel = currentAlert;

    if (currentAlert == ALERT_CRITICAL && canSendAlert(ALERT_OVERCURRENT)) {
        String msg = formatAlertMessage(ALERT_OVERCURRENT, currentAlert, data.current.value);
        if (sendSMS(ADMIN_PHONE, msg)) {
            recordAlertSent(ALERT_OVERCURRENT);
        }
    } else if (currentAlert == ALERT_OK) {
        resetAlertCooldown(ALERT_OVERCURRENT);
    }
}

// ============================================================================
// GET SUMMARY OF ACTIVE ALERTS
// ============================================================================
String getAlertSummary() {
    String summary = "";
    int activeCount = 0;

    for (int i = 0; i < ALERT_TYPE_COUNT; i++) {
        if (alertCooldowns.alertActive[i]) {
            if (activeCount > 0) summary += ", ";
            summary += getAlertTypeName((AlertType)i);
            activeCount++;
        }
    }

    if (activeCount == 0) {
        return "No active alerts";
    }

    return "Active alerts: " + summary;
}

#endif // ALERTS_H
