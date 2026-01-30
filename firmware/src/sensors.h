/**
 * @file sensors.h
 * @brief Sensor reading interface
 *
 * Handles reading from NTC thermistors, voltage sensor, current sensor,
 * and pressure transducers. Includes simulation mode for testing.
 */

#ifndef SENSORS_H
#define SENSORS_H

#include <Arduino.h>
#include "../config.h"
#include "types.h"

// =============================================================================
// FUNCTION DECLARATIONS
// =============================================================================

/**
 * @brief Initialize all sensor pins and ADC configuration
 */
void initSensors();

/**
 * @brief Read all sensors and return system data snapshot
 * @return SystemData structure with all sensor readings
 * @note Returns simulated data if SIMULATION_MODE is true
 */
SystemData readAllSensors();

/**
 * @brief Generate simulated sensor values for testing
 * @return SystemData structure with realistic test values
 */
SystemData simulateSensors();

/**
 * @brief Read temperature from NTC thermistor
 * @param pin ADC pin connected to thermistor voltage divider
 * @return Temperature in Celsius, or NAN on error
 */
float readTemperature(int pin);

/**
 * @brief Read AC voltage RMS from ZMPT101B sensor
 * @param pin ADC pin connected to voltage sensor
 * @return Voltage in Volts AC RMS
 */
float readVoltageRMS(int pin);

/**
 * @brief Read AC current RMS from ACS712 sensor
 * @param pin ADC pin connected to current sensor
 * @return Current in Amps RMS
 */
float readCurrentRMS(int pin);

/**
 * @brief Read pressure from transducer
 * @param pin ADC pin connected to pressure transducer
 * @return Pressure in PSI
 */
float readPressure(int pin);

/**
 * @brief Check if a sensor reading is within valid range
 * @param value The sensor value to check
 * @param minValid Minimum valid value
 * @param maxValid Maximum valid value
 * @return true if value is within valid range
 */
bool isValidReading(float value, float minValid, float maxValid);

/**
 * @brief Print sensor data to serial monitor for debugging
 * @param data SystemData structure to print
 */
void printSensorData(const SystemData& data);

#endif // SENSORS_H
