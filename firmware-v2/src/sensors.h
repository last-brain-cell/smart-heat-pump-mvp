/**
 * @file sensors.h
 * @brief V2 sensor reading interface
 *
 * Handles reading from DS18B20 temperature sensors (OneWire) and
 * PZEM-004T v3.0 power modules (Modbus RTU). Includes simulation mode.
 */

#ifndef SENSORS_H
#define SENSORS_H

#include <Arduino.h>
#include "../config.h"
#include "types.h"

void initSensors();
SystemData readAllSensors();
SystemData simulateSensors();
void printSensorData(const SystemData& data);
bool isValidReading(float value, float minValid, float maxValid);

#endif // SENSORS_H
