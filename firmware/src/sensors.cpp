/**
 * @file sensors.cpp
 * @brief Sensor reading implementation
 */

#include "sensors.h"
#include <math.h>

// =============================================================================
// IMPLEMENTATION
// =============================================================================

void initSensors() {
    // Configure ADC
    analogReadResolution(ADC_RESOLUTION_BITS);
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

bool isValidReading(float value, float minValid, float maxValid) {
    return !isnan(value) && value >= minValid && value <= maxValid;
}

SystemData readAllSensors() {
    if (SIMULATION_MODE) {
        return simulateSensors();
    }

    SystemData data;
    data.readingTime = millis();

    // Read temperatures
    data.tempInlet.value = readTemperature(PIN_TEMP_INLET);
    data.tempInlet.timestamp = data.readingTime;
    data.tempInlet.valid = isValidReading(data.tempInlet.value, TEMP_MIN_VALID, TEMP_MAX_VALID);

    data.tempOutlet.value = readTemperature(PIN_TEMP_OUTLET);
    data.tempOutlet.timestamp = data.readingTime;
    data.tempOutlet.valid = isValidReading(data.tempOutlet.value, TEMP_MIN_VALID, TEMP_MAX_VALID);

    data.tempAmbient.value = readTemperature(PIN_TEMP_AMBIENT);
    data.tempAmbient.timestamp = data.readingTime;
    data.tempAmbient.valid = isValidReading(data.tempAmbient.value, TEMP_MIN_VALID, TEMP_MAX_VALID);

    data.tempCompressor.value = readTemperature(PIN_TEMP_COMPRESSOR);
    data.tempCompressor.timestamp = data.readingTime;
    data.tempCompressor.valid = isValidReading(data.tempCompressor.value, TEMP_MIN_VALID, TEMP_MAX_VALID);

    // Read electrical
    data.voltage.value = readVoltageRMS(PIN_VOLTAGE);
    data.voltage.timestamp = data.readingTime;
    data.voltage.valid = isValidReading(data.voltage.value, VOLTAGE_MIN_VALID, VOLTAGE_MAX_VALID);

    data.current.value = readCurrentRMS(PIN_CURRENT);
    data.current.timestamp = data.readingTime;
    data.current.valid = isValidReading(data.current.value, CURRENT_MIN_VALID, CURRENT_MAX_VALID);

    // Calculate power
    if (data.voltage.valid && data.current.valid) {
        data.power = data.voltage.value * data.current.value;
    } else {
        data.power = 0.0f;
    }

    // Read pressure (optional sensors)
    data.pressureHigh.value = readPressure(PIN_PRESSURE_HIGH);
    data.pressureHigh.timestamp = data.readingTime;
    data.pressureHigh.valid = isValidReading(data.pressureHigh.value, PRESSURE_MIN_VALID, PRESSURE_MAX_VALID);

    data.pressureLow.value = readPressure(PIN_PRESSURE_LOW);
    data.pressureLow.timestamp = data.readingTime;
    data.pressureLow.valid = isValidReading(data.pressureLow.value, PRESSURE_MIN_VALID, PRESSURE_MAX_VALID);

    // Determine compressor running status based on current draw
    data.compressorRunning = data.current.valid && (data.current.value > 1.0f);

    return data;
}

SystemData simulateSensors() {
    SystemData data;
    data.readingTime = millis();

    // Generate small random variation (-1.0 to +1.0)
    float variation = (random(-100, 100) / 100.0f);

    // Temperatures (typical heat pump operation)
    data.tempInlet.value = 45.0f + variation;
    data.tempInlet.timestamp = data.readingTime;
    data.tempInlet.valid = true;

    data.tempOutlet.value = 50.0f + variation;
    data.tempOutlet.timestamp = data.readingTime;
    data.tempOutlet.valid = true;

    data.tempAmbient.value = 25.0f + variation;
    data.tempAmbient.timestamp = data.readingTime;
    data.tempAmbient.valid = true;

    data.tempCompressor.value = 70.0f + variation * 2.0f;
    data.tempCompressor.timestamp = data.readingTime;
    data.tempCompressor.valid = true;

    // Electrical (normal Indian grid voltage)
    data.voltage.value = 230.0f + variation * 5.0f;
    data.voltage.timestamp = data.readingTime;
    data.voltage.valid = true;

    data.current.value = 8.5f + variation * 0.5f;
    data.current.timestamp = data.readingTime;
    data.current.valid = true;

    // Calculate power
    data.power = data.voltage.value * data.current.value;

    // Pressure (typical heat pump refrigerant pressures)
    data.pressureHigh.value = 280.0f + variation * 10.0f;
    data.pressureHigh.timestamp = data.readingTime;
    data.pressureHigh.valid = true;

    data.pressureLow.value = 70.0f + variation * 5.0f;
    data.pressureLow.timestamp = data.readingTime;
    data.pressureLow.valid = true;

    // System status
    data.compressorRunning = true;
    data.fanRunning = true;
    data.defrostActive = false;

    return data;
}

float readTemperature(int pin) {
    int rawADC = analogRead(pin);

    // Prevent divide by zero and handle rail conditions
    if (rawADC <= 0) {
        return NAN;  // Sensor disconnected or shorted to ground
    }
    if (rawADC >= ADC_MAX_VALUE) {
        return NAN;  // Sensor disconnected or shorted to VCC
    }

    // Calculate voltage from ADC reading
    float voltage = rawADC * ADC_REFERENCE_VOLTAGE / ADC_MAX_VALUE;

    // Calculate NTC resistance from voltage divider
    // Circuit: 3.3V -- [NTC] -- ADC -- [10K Series] -- GND
    float resistance = NTC_SERIES_RESISTANCE * voltage / (ADC_REFERENCE_VOLTAGE - voltage);

    // Validate resistance is reasonable
    if (resistance <= 0 || resistance > 1000000) {
        return NAN;
    }

    // Steinhart-Hart equation with B parameter (simplified)
    // 1/T = 1/T0 + (1/B) * ln(R/R0)
    float steinhart = log(resistance / NTC_NOMINAL_RESISTANCE) / NTC_BETA;
    steinhart += 1.0f / (NTC_NOMINAL_TEMP + 273.15f);
    float temperature = (1.0f / steinhart) - 273.15f;

    return temperature;
}

float readVoltageRMS(int pin) {
    int64_t sumSquares = 0;
    int sample;

    // Sample the AC waveform
    for (int i = 0; i < VOLTAGE_SAMPLES; i++) {
        sample = analogRead(pin);
        // Center around zero (assuming 1.65V DC offset)
        sample -= ADC_CENTER_VALUE;
        sumSquares += (int64_t)sample * sample;
        delayMicroseconds(200);  // ~50 samples per 50Hz cycle
    }

    // Calculate RMS
    float rms = sqrt((float)sumSquares / VOLTAGE_SAMPLES);

    // Convert to voltage using calibration factor
    float voltage = rms * VOLTAGE_SCALE_FACTOR / ADC_MAX_VALUE * ADC_REFERENCE_VOLTAGE;

    return voltage;
}

float readCurrentRMS(int pin) {
    int64_t sumSquares = 0;
    int sample;
    int zeroPoint = (int)(ACS712_ZERO_POINT * ADC_MAX_VALUE / ADC_REFERENCE_VOLTAGE);

    // Sample the AC waveform
    for (int i = 0; i < CURRENT_SAMPLES; i++) {
        sample = analogRead(pin);
        // Center around zero point
        sample -= zeroPoint;
        sumSquares += (int64_t)sample * sample;
        delayMicroseconds(200);  // ~50 samples per 50Hz cycle
    }

    // Calculate RMS
    float rms = sqrt((float)sumSquares / CURRENT_SAMPLES);

    // Convert to current
    float voltageRMS = rms * ADC_REFERENCE_VOLTAGE / ADC_MAX_VALUE;
    float current = voltageRMS / ACS712_SENSITIVITY;

    return current;
}

float readPressure(int pin) {
    int rawADC = analogRead(pin);

    // Convert to voltage
    float voltage = rawADC * ADC_REFERENCE_VOLTAGE / ADC_MAX_VALUE;

    // Clamp to valid sensor range
    if (voltage < PRESSURE_MIN_VOLTAGE) {
        voltage = PRESSURE_MIN_VOLTAGE;
    }
    if (voltage > PRESSURE_MAX_VOLTAGE) {
        voltage = PRESSURE_MAX_VOLTAGE;
    }

    // Linear interpolation: 0.5V = 0 PSI, 4.5V = 500 PSI
    float pressure = (voltage - PRESSURE_MIN_VOLTAGE) /
                     (PRESSURE_MAX_VOLTAGE - PRESSURE_MIN_VOLTAGE) *
                     PRESSURE_MAX_PSI;

    return pressure;
}

void printSensorData(const SystemData& data) {
    Serial.println(F("========== SENSOR READINGS =========="));
    Serial.print(F("Time: "));
    Serial.println(data.readingTime);

    Serial.println(F("--- Temperatures (C) ---"));
    Serial.print(F("  Inlet:      "));
    Serial.print(data.tempInlet.value, 1);
    Serial.println(data.tempInlet.valid ? "" : " [INVALID]");

    Serial.print(F("  Outlet:     "));
    Serial.print(data.tempOutlet.value, 1);
    Serial.println(data.tempOutlet.valid ? "" : " [INVALID]");

    Serial.print(F("  Ambient:    "));
    Serial.print(data.tempAmbient.value, 1);
    Serial.println(data.tempAmbient.valid ? "" : " [INVALID]");

    Serial.print(F("  Compressor: "));
    Serial.print(data.tempCompressor.value, 1);
    Serial.println(data.tempCompressor.valid ? "" : " [INVALID]");

    Serial.println(F("--- Electrical ---"));
    Serial.print(F("  Voltage: "));
    Serial.print(data.voltage.value, 1);
    Serial.println(F(" V"));

    Serial.print(F("  Current: "));
    Serial.print(data.current.value, 2);
    Serial.println(F(" A"));

    Serial.print(F("  Power:   "));
    Serial.print(data.power, 0);
    Serial.println(F(" W"));

    Serial.println(F("--- Pressure (PSI) ---"));
    Serial.print(F("  High: "));
    Serial.println(data.pressureHigh.value, 0);

    Serial.print(F("  Low:  "));
    Serial.println(data.pressureLow.value, 0);

    Serial.println(F("--- Status ---"));
    Serial.print(F("  Compressor: "));
    Serial.println(data.compressorRunning ? "ON" : "OFF");

    Serial.println(F("====================================="));
}
