/**
 * @file alerts.h
 * @brief Alert management interface
 *
 * Handles threshold checking for all sensors and manages alert cooldowns
 * to prevent SMS spam.
 */

#ifndef ALERTS_H
#define ALERTS_H

#include <Arduino.h>
#include "../config.h"
#include "types.h"

// =============================================================================
// FUNCTION DECLARATIONS
// =============================================================================

/**
 * @brief Initialize the alert system
 */
void initAlerts();

/**
 * @brief Check voltage against thresholds
 * @param voltage Voltage reading in Volts
 * @param isHigh Output: true if voltage is high, false if low
 * @return Alert level (OK, WARNING, or CRITICAL)
 */
AlertLevel checkVoltage(float voltage, bool* isHigh);

/**
 * @brief Check compressor temperature against thresholds
 * @param temp Temperature in Celsius
 * @return Alert level
 */
AlertLevel checkCompressorTemp(float temp);

/**
 * @brief Check high-side pressure against thresholds
 * @param pressure Pressure in PSI
 * @return Alert level
 */
AlertLevel checkPressureHigh(float pressure);

/**
 * @brief Check low-side pressure against thresholds
 * @param pressure Pressure in PSI
 * @return Alert level
 */
AlertLevel checkPressureLow(float pressure);

/**
 * @brief Check current against overcurrent threshold
 * @param current Current in Amps
 * @return Alert level
 */
AlertLevel checkCurrent(float current);

/**
 * @brief Check if an alert can be sent (cooldown check)
 * @param type Alert type to check
 * @return true if cooldown period has passed
 */
bool canSendAlert(AlertType type);

/**
 * @brief Record that an alert was sent
 * @param type Alert type that was sent
 */
void recordAlertSent(AlertType type);

/**
 * @brief Reset cooldown when condition clears
 * @param type Alert type to reset
 */
void resetAlertCooldown(AlertType type);

/**
 * @brief Format an alert message for SMS
 * @param type Alert type
 * @param level Alert severity
 * @param value Current sensor value
 * @param buffer Output buffer
 * @param bufferSize Size of output buffer
 * @return Number of characters written
 */
size_t formatAlertMessage(AlertType type, AlertLevel level, float value,
                          char* buffer, size_t bufferSize);

/**
 * @brief Check all sensors and send alerts if needed
 * @param data System data to check (alertLevel fields will be updated)
 */
void checkAllAlerts(SystemData& data);

/**
 * @brief Get summary of active alerts
 * @param buffer Output buffer
 * @param bufferSize Size of output buffer
 * @return Number of characters written
 */
size_t getAlertSummary(char* buffer, size_t bufferSize);

#endif // ALERTS_H
