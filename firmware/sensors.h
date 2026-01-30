/*
 * sensors.h - Sensor reading functions
 *
 * Handles reading from NTC thermistors, voltage sensor, current sensor,
 * and pressure transducers. Includes simulation mode for testing.
 */

#ifndef SENSORS_H
#define SENSORS_H

#include <Arduino.h>
#include "config.h"
#include "types.h"

// ============================================================================
// FUNCTION DECLARATIONS
// ============================================================================
void initSensors();
SystemData readAllSensors();
SystemData simulateSensors();

float readTemperature(int pin);
float readVoltageRMS(int pin);
float readCurrentRMS(int pin);
float readPressure(int pin);

// ============================================================================
// INITIALIZE SENSORS
// ============================================================================
void initSensors() {
    // Configure ADC
    analogReadResolution(12);  // 12-bit resolution (0-4095)
    analogSetAttenuation(ADC_11db);  // Full range 0-3.3V

    // Set pin modes for analog inputs
    pinMode(PIN_TEMP_INLET, INPUT);
    pinMode(PIN_TEMP_OUTLET, INPUT);
    pinMode(PIN_TEMP_AMBIENT, INPUT);
    pinMode(PIN_TEMP_COMPRESSOR, INPUT);
    pinMode(PIN_VOLTAGE, INPUT);
    pinMode(PIN_CURRENT, INPUT);
    pinMode(PIN_PRESSURE_HIGH, INPUT);
    pinMode(PIN_PRESSURE_LOW, INPUT);

    // Status LED
    pinMode(PIN_STATUS_LED, OUTPUT);
    digitalWrite(PIN_STATUS_LED, LOW);

    Serial.println(F("[SENSORS] Initialized"));
}

// ============================================================================
// READ ALL SENSORS
// Returns simulated data if SIMULATION_MODE is true
// ============================================================================
SystemData readAllSensors() {
    if (SIMULATION_MODE) {
        return simulateSensors();
    }

    SystemData data;
    data.readingTime = millis();

    // Read temperatures
    data.tempInlet.value = readTemperature(PIN_TEMP_INLET);
    data.tempInlet.timestamp = data.readingTime;
    data.tempInlet.valid = (data.tempInlet.value > -40 && data.tempInlet.value < 125);

    data.tempOutlet.value = readTemperature(PIN_TEMP_OUTLET);
    data.tempOutlet.timestamp = data.readingTime;
    data.tempOutlet.valid = (data.tempOutlet.value > -40 && data.tempOutlet.value < 125);

    data.tempAmbient.value = readTemperature(PIN_TEMP_AMBIENT);
    data.tempAmbient.timestamp = data.readingTime;
    data.tempAmbient.valid = (data.tempAmbient.value > -40 && data.tempAmbient.value < 125);

    data.tempCompressor.value = readTemperature(PIN_TEMP_COMPRESSOR);
    data.tempCompressor.timestamp = data.readingTime;
    data.tempCompressor.valid = (data.tempCompressor.value > -40 && data.tempCompressor.value < 125);

    // Read electrical
    data.voltage.value = readVoltageRMS(PIN_VOLTAGE);
    data.voltage.timestamp = data.readingTime;
    data.voltage.valid = (data.voltage.value > 0 && data.voltage.value < 300);

    data.current.value = readCurrentRMS(PIN_CURRENT);
    data.current.timestamp = data.readingTime;
    data.current.valid = (data.current.value >= 0 && data.current.value < 25);

    // Calculate power
    data.power = data.voltage.value * data.current.value;

    // Read pressure (optional sensors)
    data.pressureHigh.value = readPressure(PIN_PRESSURE_HIGH);
    data.pressureHigh.timestamp = data.readingTime;
    data.pressureHigh.valid = (data.pressureHigh.value >= 0 && data.pressureHigh.value <= 500);

    data.pressureLow.value = readPressure(PIN_PRESSURE_LOW);
    data.pressureLow.timestamp = data.readingTime;
    data.pressureLow.valid = (data.pressureLow.value >= 0 && data.pressureLow.value <= 500);

    // Determine compressor running status based on current draw
    data.compressorRunning = (data.current.value > 1.0);

    return data;
}

// ============================================================================
// SIMULATE SENSOR VALUES
// Returns realistic test data for development without hardware
// ============================================================================
SystemData simulateSensors() {
    SystemData data;
    data.readingTime = millis();

    // Simulate realistic heat pump values with small random variations
    float variation = (random(-100, 100) / 100.0);  // -1.0 to +1.0

    // Temperatures (typical heat pump operation)
    data.tempInlet.value = 45.0 + variation;      // Warm water inlet
    data.tempInlet.timestamp = data.readingTime;
    data.tempInlet.valid = true;

    data.tempOutlet.value = 50.0 + variation;     // Warmer outlet
    data.tempOutlet.timestamp = data.readingTime;
    data.tempOutlet.valid = true;

    data.tempAmbient.value = 25.0 + variation;    // Room temperature
    data.tempAmbient.timestamp = data.readingTime;
    data.tempAmbient.valid = true;

    data.tempCompressor.value = 70.0 + variation * 2;  // Compressor runs warm
    data.tempCompressor.timestamp = data.readingTime;
    data.tempCompressor.valid = true;

    // Electrical (normal Indian grid voltage)
    data.voltage.value = 230.0 + variation * 5;   // 225-235V typical
    data.voltage.timestamp = data.readingTime;
    data.voltage.valid = true;

    data.current.value = 8.5 + variation * 0.5;   // Running load
    data.current.timestamp = data.readingTime;
    data.current.valid = true;

    // Calculate power
    data.power = data.voltage.value * data.current.value;

    // Pressure (typical heat pump refrigerant pressures)
    data.pressureHigh.value = 280.0 + variation * 10;  // High side ~280 PSI
    data.pressureHigh.timestamp = data.readingTime;
    data.pressureHigh.valid = true;

    data.pressureLow.value = 70.0 + variation * 5;     // Low side ~70 PSI
    data.pressureLow.timestamp = data.readingTime;
    data.pressureLow.valid = true;

    // System status
    data.compressorRunning = true;
    data.fanRunning = true;
    data.defrostActive = false;

    return data;
}

// ============================================================================
// READ NTC THERMISTOR TEMPERATURE
// Uses Steinhart-Hart equation with B-coefficient
// ============================================================================
float readTemperature(int pin) {
    int rawADC = analogRead(pin);

    // Prevent divide by zero
    if (rawADC == 0) rawADC = 1;
    if (rawADC >= 4095) rawADC = 4094;

    // Calculate resistance of NTC
    // Assuming voltage divider: 3.3V -- [NTC] -- ADC -- [10K] -- GND
    float voltage = rawADC * 3.3 / 4095.0;
    float resistance = NTC_SERIES_RESISTANCE * voltage / (3.3 - voltage);

    // Steinhart-Hart with B parameter (simplified)
    // 1/T = 1/T0 + (1/B) * ln(R/R0)
    float steinhart = log(resistance / NTC_NOMINAL_RESISTANCE) / NTC_BETA;
    steinhart += 1.0 / (NTC_NOMINAL_TEMP + 273.15);
    float temperature = (1.0 / steinhart) - 273.15;

    return temperature;
}

// ============================================================================
// READ AC VOLTAGE (RMS) FROM ZMPT101B
// Takes multiple samples to calculate RMS value
// ============================================================================
float readVoltageRMS(int pin) {
    long sumSquares = 0;
    int sample;

    // Sample the AC waveform
    for (int i = 0; i < VOLTAGE_SAMPLES; i++) {
        sample = analogRead(pin);
        // Center around zero (assuming 1.65V DC offset for 3.3V reference)
        sample -= 2048;  // Center of 12-bit range
        sumSquares += (long)sample * sample;
        delayMicroseconds(200);  // ~50 samples per 50Hz cycle
    }

    // Calculate RMS
    float rms = sqrt(sumSquares / VOLTAGE_SAMPLES);

    // Convert to voltage using calibration factor
    // The scale factor should be calibrated with a known voltage source
    float voltage = rms * VOLTAGE_SCALE_FACTOR / 4095.0 * 3.3;

    return voltage;
}

// ============================================================================
// READ AC CURRENT (RMS) FROM ACS712
// Takes multiple samples to calculate RMS value
// ============================================================================
float readCurrentRMS(int pin) {
    long sumSquares = 0;
    int sample;

    // Sample the AC waveform
    for (int i = 0; i < CURRENT_SAMPLES; i++) {
        sample = analogRead(pin);
        // Center around zero point (ACS712 outputs 2.5V at 0A, but with 3.3V ref it's ~1.65V)
        sample -= (int)(ACS712_ZERO_POINT * 4095.0 / 3.3);
        sumSquares += (long)sample * sample;
        delayMicroseconds(200);  // ~50 samples per 50Hz cycle
    }

    // Calculate RMS
    float rms = sqrt(sumSquares / CURRENT_SAMPLES);

    // Convert to current
    // ADC value to voltage, then voltage to current
    float voltageRMS = rms * 3.3 / 4095.0;
    float current = voltageRMS / ACS712_SENSITIVITY;

    return current;
}

// ============================================================================
// READ PRESSURE FROM 0-500 PSI TRANSDUCER
// Linear mapping from 0.5-4.5V to 0-500 PSI
// ============================================================================
float readPressure(int pin) {
    int rawADC = analogRead(pin);

    // Convert to voltage (3.3V reference)
    float voltage = rawADC * 3.3 / 4095.0;

    // Linear interpolation: 0.5V = 0 PSI, 4.5V = 500 PSI
    if (voltage < PRESSURE_MIN_VOLTAGE) voltage = PRESSURE_MIN_VOLTAGE;
    if (voltage > PRESSURE_MAX_VOLTAGE) voltage = PRESSURE_MAX_VOLTAGE;

    float pressure = (voltage - PRESSURE_MIN_VOLTAGE) /
                     (PRESSURE_MAX_VOLTAGE - PRESSURE_MIN_VOLTAGE) *
                     PRESSURE_MAX_PSI;

    return pressure;
}

// ============================================================================
// PRINT SENSOR DATA TO SERIAL (for debugging)
// ============================================================================
void printSensorData(const SystemData& data) {
    Serial.println(F("========== SENSOR READINGS =========="));
    Serial.print(F("Time: ")); Serial.println(data.readingTime);

    Serial.println(F("--- Temperatures (C) ---"));
    Serial.print(F("  Inlet:      ")); Serial.println(data.tempInlet.value, 1);
    Serial.print(F("  Outlet:     ")); Serial.println(data.tempOutlet.value, 1);
    Serial.print(F("  Ambient:    ")); Serial.println(data.tempAmbient.value, 1);
    Serial.print(F("  Compressor: ")); Serial.println(data.tempCompressor.value, 1);

    Serial.println(F("--- Electrical ---"));
    Serial.print(F("  Voltage: ")); Serial.print(data.voltage.value, 1); Serial.println(F(" V"));
    Serial.print(F("  Current: ")); Serial.print(data.current.value, 2); Serial.println(F(" A"));
    Serial.print(F("  Power:   ")); Serial.print(data.power, 0); Serial.println(F(" W"));

    Serial.println(F("--- Pressure (PSI) ---"));
    Serial.print(F("  High: ")); Serial.println(data.pressureHigh.value, 0);
    Serial.print(F("  Low:  ")); Serial.println(data.pressureLow.value, 0);

    Serial.println(F("--- Status ---"));
    Serial.print(F("  Compressor: ")); Serial.println(data.compressorRunning ? "ON" : "OFF");
    Serial.println(F("====================================="));
}

#endif // SENSORS_H
