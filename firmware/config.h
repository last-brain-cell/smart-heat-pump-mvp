/**
 * @file config.h
 * @brief Configuration constants for Heat Pump Monitor
 *
 * Modify these values for your specific installation.
 * Fields marked [REQUIRED] must be changed before deployment.
 */

#ifndef CONFIG_H
#define CONFIG_H

// =============================================================================
// FIRMWARE VERSION
// =============================================================================
#define FIRMWARE_VERSION_MAJOR 1
#define FIRMWARE_VERSION_MINOR 0
#define FIRMWARE_VERSION_PATCH 0
#define FIRMWARE_VERSION "1.0.0"

// =============================================================================
// DEVICE IDENTIFICATION
// =============================================================================
/** @brief Unique device identifier - used in MQTT topics and SMS messages */
#define DEVICE_ID "site1"

// =============================================================================
// SIMULATION MODE
// =============================================================================
/**
 * @brief Enable simulated sensor values for testing without hardware
 * Set to false for production deployment with real sensors
 */
#define SIMULATION_MODE false

// =============================================================================
// [REQUIRED] ADMIN PHONE NUMBER
// =============================================================================
/**
 * @brief Phone number for SMS alerts (with country code)
 * @note Change this before deployment!
 */
#define ADMIN_PHONE "+917722087410"

// Validation marker for configuration check
#define ADMIN_PHONE_PLACEHOLDER "+919876543210"

// =============================================================================
// [REQUIRED] GSM/GPRS SETTINGS
// =============================================================================
#define GSM_PIN ""              ///< SIM PIN if required (usually empty)
#define APN "airtelgprs.com"    ///< Carrier APN (Airtel: "airtelgprs.com", Vi: "portalnmms")
#define GPRS_USER ""            ///< APN username (usually empty for Indian carriers)
#define GPRS_PASS ""            ///< APN password (usually empty for Indian carriers)

// =============================================================================
// WIFI SETTINGS — compile-time defaults, overridden by portal config in NVS
// =============================================================================
#define WIFI_SSID "Airtel_Dantales-wifi"        ///< WiFi network name
#define WIFI_PASS_KEY "9823807410" ///< WiFi password
#define WIFI_CONNECT_TIMEOUT 10000UL      ///< 10 seconds - WiFi connection timeout

// =============================================================================
// MQTT BROKER SETTINGS — compile-time defaults, overridden by portal config in NVS
// =============================================================================
#define MQTT_BROKER "192.168.1.7"   ///< MQTT broker hostname or IP
#define MQTT_PORT 1883                   ///< MQTT broker port
#define MQTT_USER "heatpump"             ///< MQTT username
#define MQTT_PASS "heatpump123"             ///< MQTT password
#define MQTT_CLIENT_ID DEVICE_ID         ///< MQTT client identifier

// Validation markers for configuration check
#define MQTT_BROKER_PLACEHOLDER "localhost"
#define MQTT_PASS_PLACEHOLDER "heatpump123"

// =============================================================================
// TIMING INTERVALS (milliseconds)
// =============================================================================
#define SENSOR_READ_INTERVAL    10000UL   ///< 10 seconds - sensor polling
#define MQTT_PUBLISH_INTERVAL   10000UL  ///< 10 seconds - MQTT data publish (INCREASE LATER: REDUCED FOR PROTOTYPE)
#define ALERT_COOLDOWN          300000UL  ///< 5 minutes - between same alerts
#define SMS_CHECK_INTERVAL      5000UL    ///< 5 seconds - check for SMS
#define GPRS_RETRY_INTERVAL     60000UL   ///< 1 minute - between GPRS retries
#define WIFI_RETRY_INTERVAL     60000UL   ///< 1 minute - between WiFi retries
#define NETWORK_TIMEOUT         60000UL   ///< 1 minute - wait for network
#define WATCHDOG_TIMEOUT_S      30        ///< 30 seconds - watchdog timeout

// =============================================================================
// PIN DEFINITIONS - GSM Module (SIM800C)
// =============================================================================
#define PIN_GSM_RX  16    ///< ESP32 GPIO16 <- SIM800C TX
#define PIN_GSM_TX  17    ///< ESP32 GPIO17 -> SIM800C RX
#define GSM_BAUD    9600  ///< SIM800C default baud rate

// =============================================================================
// PIN DEFINITIONS - Temperature Sensors (10K NTC Thermistors)
// Using ADC1 pins (safe to use with WiFi/GSM active)
// =============================================================================
#define PIN_TEMP_INLET       34    ///< ADC1_CH6 - Water/refrigerant inlet
#define PIN_TEMP_OUTLET      35    ///< ADC1_CH7 - Water/refrigerant outlet
#define PIN_TEMP_AMBIENT     32    ///< ADC1_CH4 - Ambient air temperature
#define PIN_TEMP_COMPRESSOR  33    ///< ADC1_CH5 - Compressor body temperature

// =============================================================================
// PIN DEFINITIONS - Electrical Sensors
// =============================================================================
#define PIN_VOLTAGE  36    ///< ADC1_CH0 (VP) - ZMPT101B AC voltage sensor
#define PIN_CURRENT  39    ///< ADC1_CH3 (VN) - ACS712-20A current sensor

// =============================================================================
// PIN DEFINITIONS - Pressure Sensors (Optional)
// Using ADC2 pins (may have issues during WiFi TX, acceptable for optional sensors)
// =============================================================================
#define PIN_PRESSURE_HIGH  25    ///< ADC2_CH8 - High side pressure transducer
#define PIN_PRESSURE_LOW   26    ///< ADC2_CH9 - Low side pressure transducer

// =============================================================================
// PIN DEFINITIONS - Status LED
// =============================================================================
#define PIN_STATUS_LED  2    ///< Built-in LED on most ESP32 dev boards

// =============================================================================
// SENSOR CALIBRATION CONSTANTS
// =============================================================================

// NTC Thermistor (10K)
#define NTC_BETA              3950.0f   ///< B-coefficient (verify with datasheet)
#define NTC_NOMINAL_RESISTANCE 10000.0f ///< Resistance at 25C
#define NTC_NOMINAL_TEMP      25.0f     ///< Temperature for nominal resistance
#define NTC_SERIES_RESISTANCE 10000.0f  ///< Series resistor value

// ZMPT101B AC Voltage Sensor
#define VOLTAGE_SAMPLES       500       ///< Number of samples for RMS calculation
#define VOLTAGE_SCALE_FACTOR  234.26f   ///< Calibrate with known voltage source

// ACS712-20A Current Sensor
#define ACS712_SENSITIVITY    0.100f    ///< 100mV per Amp for 20A version
#define ACS712_ZERO_POINT     1.65f     ///< ~1.65V at 0A (with 3.3V reference)
#define CURRENT_SAMPLES       500       ///< Number of samples for RMS calculation

// Pressure Transducer (0.5-4.5V = 0-500 PSI)
#define PRESSURE_MIN_VOLTAGE  0.5f
#define PRESSURE_MAX_VOLTAGE  4.5f
#define PRESSURE_MAX_PSI      500.0f

// ADC Configuration
#define ADC_RESOLUTION_BITS   12
#define ADC_MAX_VALUE         4095
#define ADC_REFERENCE_VOLTAGE 3.3f
#define ADC_CENTER_VALUE      2048      ///< Midpoint for AC measurements

// =============================================================================
// ALERT THRESHOLDS
// =============================================================================

// Voltage thresholds (Volts AC)
#define VOLTAGE_HIGH_CRITICAL  250.0f
#define VOLTAGE_HIGH_WARNING   245.0f
#define VOLTAGE_LOW_WARNING    215.0f
#define VOLTAGE_LOW_CRITICAL   210.0f

// Compressor temperature thresholds (Celsius)
#define COMP_TEMP_CRITICAL     95.0f
#define COMP_TEMP_WARNING      85.0f

// High side pressure thresholds (PSI)
#define PRESSURE_HIGH_CRITICAL 450.0f
#define PRESSURE_HIGH_WARNING  400.0f

// Low side pressure thresholds (PSI)
#define PRESSURE_LOW_CRITICAL  20.0f
#define PRESSURE_LOW_WARNING   40.0f

// Current thresholds (Amps)
#define CURRENT_CRITICAL       15.0f
#define CURRENT_WARNING        12.0f

// Sensor validity ranges
#define TEMP_MIN_VALID        -40.0f
#define TEMP_MAX_VALID        125.0f
#define VOLTAGE_MIN_VALID     0.0f
#define VOLTAGE_MAX_VALID     300.0f
#define CURRENT_MIN_VALID     0.0f
#define CURRENT_MAX_VALID     25.0f
#define PRESSURE_MIN_VALID    0.0f
#define PRESSURE_MAX_VALID    500.0f

// =============================================================================
// DATA BUFFER SETTINGS
// =============================================================================
#define BUFFER_SIZE  100    ///< Maximum readings to store when offline

// =============================================================================
// MQTT TOPICS
// =============================================================================
#define MQTT_TOPIC_BASE "heatpump/" DEVICE_ID

// =============================================================================
// MESSAGE BUFFER SIZES
// =============================================================================
#define SMS_BUFFER_SIZE     160   ///< Max SMS message length
#define JSON_BUFFER_SIZE    1024  ///< JSON payload buffer size
#define STATUS_BUFFER_SIZE  256   ///< Status message buffer size

#endif // CONFIG_H
