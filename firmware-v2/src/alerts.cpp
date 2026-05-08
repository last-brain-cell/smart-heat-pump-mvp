/**
 * @file alerts.cpp
 * @brief V2 per-phase alert management implementation
 *
 * Checks voltage (high/low) and overcurrent for each of 3 phases.
 * 9 alert types total with independent cooldowns.
 */

#include "alerts.h"
#include "globals.h"
#include "gsm.h"

// =============================================================================
// PRIVATE DATA
// =============================================================================

static AlertCooldown alertCooldowns;

// =============================================================================
// PRIVATE HELPERS
// =============================================================================

static AlertLevel checkVoltage(float voltage) {
    if (voltage >= VOLTAGE_HIGH_CRITICAL) return ALERT_CRITICAL;
    if (voltage >= VOLTAGE_HIGH_WARNING)  return ALERT_WARNING;
    if (voltage <= VOLTAGE_LOW_CRITICAL)  return ALERT_CRITICAL;
    if (voltage <= VOLTAGE_LOW_WARNING)   return ALERT_WARNING;
    return ALERT_OK;
}

static AlertLevel checkCurrent(float current) {
    if (current >= CURRENT_CRITICAL) return ALERT_CRITICAL;
    if (current >= CURRENT_WARNING)  return ALERT_WARNING;
    return ALERT_OK;
}

// =============================================================================
// IMPLEMENTATION
// =============================================================================

void initAlerts() {
    for (int i = 0; i < ALERT_TYPE_COUNT; i++) {
        alertCooldowns.lastAlertTime[i] = 0;
        alertCooldowns.alertActive[i] = false;
    }
    Log.println(F("[ALERTS] V2 alerts initialized (3-phase, 9 types)"));
}

bool canSendAlert(AlertType type) {
    if (type >= ALERT_TYPE_COUNT) return false;

    unsigned long now = millis();
    unsigned long last = alertCooldowns.lastAlertTime[type];

    // Handle millis() overflow
    if (now < last) {
        alertCooldowns.lastAlertTime[type] = 0;
        return true;
    }

    return (now - last) >= ALERT_COOLDOWN;
}

void recordAlertSent(AlertType type) {
    if (type >= ALERT_TYPE_COUNT) return;

    alertCooldowns.lastAlertTime[type] = millis();
    alertCooldowns.alertActive[type] = true;

    Log.print(F("[ALERTS] Alert recorded: "));
    Log.println(getAlertTypeName(type));
}

void resetAlertCooldown(AlertType type) {
    if (type >= ALERT_TYPE_COUNT) return;

    if (alertCooldowns.alertActive[type]) {
        alertCooldowns.alertActive[type] = false;
        Log.print(F("[ALERTS] Alert cleared: "));
        Log.println(getAlertTypeName(type));
    }
}

size_t formatAlertMessage(AlertType type, AlertLevel level, float value,
                          char* buffer, size_t bufferSize) {
    const char* unit = "";
    switch (type) {
        case ALERT_VOLTAGE_HIGH_P1: case ALERT_VOLTAGE_LOW_P1:
        case ALERT_VOLTAGE_HIGH_P2: case ALERT_VOLTAGE_LOW_P2:
        case ALERT_VOLTAGE_HIGH_P3: case ALERT_VOLTAGE_LOW_P3:
            unit = "V";
            break;
        case ALERT_OVERCURRENT_P1:
        case ALERT_OVERCURRENT_P2:
        case ALERT_OVERCURRENT_P3:
            unit = "A";
            break;
        default:
            break;
    }

    return snprintf(buffer, bufferSize,
        "ALERT: %s\n"
        "Level: %s\n"
        "Value: %.1f %s\n\n"
        "Device: %s",
        getAlertTypeName(type),
        getAlertLevelName(level),
        value, unit,
        deviceId
    );
}

void checkAllAlerts(SystemData& data) {
    char buf[SMS_BUFFER_SIZE];

    AlertType voltHighTypes[] = {ALERT_VOLTAGE_HIGH_P1, ALERT_VOLTAGE_HIGH_P2, ALERT_VOLTAGE_HIGH_P3};
    AlertType voltLowTypes[]  = {ALERT_VOLTAGE_LOW_P1,  ALERT_VOLTAGE_LOW_P2,  ALERT_VOLTAGE_LOW_P3};
    AlertType currTypes[]     = {ALERT_OVERCURRENT_P1,  ALERT_OVERCURRENT_P2,  ALERT_OVERCURRENT_P3};

    for (int i = 0; i < 3; i++) {
        if (!data.phase[i].valid) continue;

        // ---- VOLTAGE CHECK (per phase) ----
        AlertLevel vAlert = checkVoltage(data.phase[i].voltage.value);
        data.phase[i].voltage.alertLevel = vAlert;

        if (vAlert != ALERT_OK) {
            bool isHigh = data.phase[i].voltage.value > 230.0f;
            AlertType at = isHigh ? voltHighTypes[i] : voltLowTypes[i];

            if (vAlert == ALERT_CRITICAL && canSendAlert(at)) {
                formatAlertMessage(at, vAlert, data.phase[i].voltage.value,
                                   buf, sizeof(buf));
                if (sendSMS(ADMIN_PHONE, buf)) {
                    recordAlertSent(at);
                }
            }
        } else {
            resetAlertCooldown(voltHighTypes[i]);
            resetAlertCooldown(voltLowTypes[i]);
        }

        // ---- CURRENT CHECK (per phase) ----
        AlertLevel cAlert = checkCurrent(data.phase[i].current.value);
        data.phase[i].current.alertLevel = cAlert;

        if (cAlert == ALERT_CRITICAL && canSendAlert(currTypes[i])) {
            formatAlertMessage(currTypes[i], cAlert, data.phase[i].current.value,
                               buf, sizeof(buf));
            if (sendSMS(ADMIN_PHONE, buf)) {
                recordAlertSent(currTypes[i]);
            }
        } else if (cAlert == ALERT_OK) {
            resetAlertCooldown(currTypes[i]);
        }
    }
}

size_t getAlertSummary(char* buffer, size_t bufferSize) {
    int activeCount = 0;
    for (int i = 0; i < ALERT_TYPE_COUNT; i++) {
        if (alertCooldowns.alertActive[i]) activeCount++;
    }

    if (activeCount == 0) {
        return snprintf(buffer, bufferSize, "No active alerts");
    }

    size_t written = snprintf(buffer, bufferSize, "Active: ");
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
