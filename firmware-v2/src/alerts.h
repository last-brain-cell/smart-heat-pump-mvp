/**
 * @file alerts.h
 * @brief V2 per-phase alert management interface
 *
 * Checks voltage and current thresholds for each of 3 phases.
 * Sends SMS alerts with cooldown to prevent spam.
 */

#ifndef ALERTS_H
#define ALERTS_H

#include <Arduino.h>
#include "../config.h"
#include "types.h"

void initAlerts();
void checkAllAlerts(SystemData& data);
bool canSendAlert(AlertType type);
void recordAlertSent(AlertType type);
void resetAlertCooldown(AlertType type);
size_t formatAlertMessage(AlertType type, AlertLevel level, float value,
                          char* buffer, size_t bufferSize);
size_t getAlertSummary(char* buffer, size_t bufferSize);

#endif // ALERTS_H
