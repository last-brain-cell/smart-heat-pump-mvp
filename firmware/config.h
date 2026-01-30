/*
 * config.h - Configuration for Heat Pump Monitor
 *
 * Modify these values for your specific installation
 */

#ifndef CONFIG_H
#define CONFIG_H

// ============================================================================
// DEVICE IDENTIFICATION
// ============================================================================
#define DEVICE_ID "site1"

// ============================================================================
// SIMULATION MODE
// Set to true to use simulated sensor values (for testing without hardware)
// Set to false to read from actual sensors
// ============================================================================
#define SIMULATION_MODE true

// ============================================================================
// ADMIN PHONE NUMBER (with country code)
// ============================================================================
#define ADMIN_PHONE "+919876543210"

// ============================================================================
// GSM/GPRS SETTINGS
// ============================================================================
#define GSM_PIN ""              // SIM PIN if required (usually empty)
#define APN "airtelgprs.com"    // Airtel India APN (change for Vi: "portalnmms")
#define GPRS_USER ""            // Usually empty for Indian carriers
#define GPRS_PASS ""            // Usually empty for Indian carriers

// ============================================================================
// MQTT BROKER SETTINGS
// ============================================================================
#define MQTT_BROKER "your-server.com"
#define MQTT_PORT 1883
#define MQTT_USER "heatpump"
#define MQTT_PASS "password"
#define MQTT_CLIENT_ID DEVICE_ID

// ============================================================================
// TIMING INTERVALS (milliseconds)
// ============================================================================
#define SENSOR_READ_INTERVAL    10000    // 10 seconds - read all sensors
#define MQTT_PUBLISH_INTERVAL   300000   // 5 minutes - publish to MQTT
#define ALERT_COOLDOWN          300000   // 5 minutes - between same alerts
#define SMS_CHECK_INTERVAL      5000     // 5 seconds - check for SMS
#define GPRS_RETRY_INTERVAL     60000    // 1 minute - between GPRS retries
#define NETWORK_TIMEOUT         60000    // 1 minute - wait for network

// ============================================================================
// PIN DEFINITIONS - GSM Module (SIM800C)
// ============================================================================
#define PIN_GSM_RX  16    // ESP32 GPIO16 receives from SIM800C TX
#define PIN_GSM_TX  17    // ESP32 GPIO17 transmits to SIM800C RX
#define GSM_BAUD    9600  // SIM800C default baud rate

// ============================================================================
// PIN DEFINITIONS - Temperature Sensors (10K NTC Thermistors)
// Using ADC1 pins (safe to use with WiFi/GSM)
// ============================================================================
#define PIN_TEMP_INLET       34    // ADC1_CH6 - Water/refrigerant inlet
#define PIN_TEMP_OUTLET      35    // ADC1_CH7 - Water/refrigerant outlet
#define PIN_TEMP_AMBIENT     32    // ADC1_CH4 - Ambient air temperature
#define PIN_TEMP_COMPRESSOR  33    // ADC1_CH5 - Compressor body temperature

// ============================================================================
// PIN DEFINITIONS - Electrical Sensors
// ============================================================================
#define PIN_VOLTAGE  36    // ADC1_CH0 (VP) - ZMPT101B AC voltage sensor
#define PIN_CURRENT  39    // ADC1_CH3 (VN) - ACS712-20A current sensor

// ============================================================================
// PIN DEFINITIONS - Pressure Sensors (Optional)
// Using ADC2 pins (may have issues during GSM TX, but acceptable for optional sensors)
// ============================================================================
#define PIN_PRESSURE_HIGH  25    // ADC2_CH8 - High side pressure transducer
#define PIN_PRESSURE_LOW   26    // ADC2_CH9 - Low side pressure transducer

// ============================================================================
// PIN DEFINITIONS - Status LED
// ============================================================================
#define PIN_STATUS_LED  2    // Built-in LED on most ESP32 dev boards

// ============================================================================
// SENSOR CALIBRATION CONSTANTS
// ============================================================================

// NTC Thermistor (10K)
#define NTC_BETA              3950.0    // B-coefficient (check datasheet)
#define NTC_NOMINAL_RESISTANCE 10000.0  // Resistance at 25C
#define NTC_NOMINAL_TEMP      25.0      // Temperature for nominal resistance
#define NTC_SERIES_RESISTANCE 10000.0   // Series resistor value

// ZMPT101B AC Voltage Sensor
#define VOLTAGE_SAMPLES       500       // Number of samples for RMS calculation
#define VOLTAGE_SCALE_FACTOR  234.26    // Calibrate with known voltage source

// ACS712-20A Current Sensor
#define ACS712_SENSITIVITY    0.100     // 100mV per Amp
#define ACS712_ZERO_POINT     1.65      // ~1.65V at 0A (with 3.3V reference)
#define CURRENT_SAMPLES       500       // Number of samples for RMS calculation

// Pressure Transducer (0.5-4.5V = 0-500 PSI)
#define PRESSURE_MIN_VOLTAGE  0.5
#define PRESSURE_MAX_VOLTAGE  4.5
#define PRESSURE_MAX_PSI      500.0

// ============================================================================
// ALERT THRESHOLDS
// ============================================================================

// Voltage thresholds (Volts AC)
#define VOLTAGE_HIGH_CRITICAL  250.0
#define VOLTAGE_HIGH_WARNING   245.0
#define VOLTAGE_LOW_WARNING    215.0
#define VOLTAGE_LOW_CRITICAL   210.0

// Compressor temperature thresholds (Celsius)
#define COMP_TEMP_CRITICAL     95.0
#define COMP_TEMP_WARNING      85.0

// High side pressure thresholds (PSI)
#define PRESSURE_HIGH_CRITICAL 450.0
#define PRESSURE_HIGH_WARNING  400.0

// Low side pressure thresholds (PSI)
#define PRESSURE_LOW_CRITICAL  20.0
#define PRESSURE_LOW_WARNING   40.0

// Current thresholds (Amps)
#define CURRENT_CRITICAL       15.0
#define CURRENT_WARNING        12.0

// ============================================================================
// DATA BUFFER SETTINGS
// ============================================================================
#define BUFFER_SIZE  100    // Maximum readings to store when offline

// ============================================================================
// MQTT TOPIC BASE
// ============================================================================
#define MQTT_TOPIC_BASE "heatpump/" DEVICE_ID

#endif // CONFIG_H
