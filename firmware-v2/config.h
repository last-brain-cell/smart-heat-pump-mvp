/**
 * @file config.h
 * @brief Configuration constants for Heat Pump Monitor V2
 *
 * V2 Hardware:
 *   - 3x PZEM-004T v3.0 (3-phase power monitoring via Modbus RTU)
 *   - 2x DS18B20 (inlet/outlet temperature via OneWire)
 *   - SIM800C GSM Module (SMS alerts + GPRS fallback)
 *
 * Modify these values for your specific installation.
 * Fields marked [REQUIRED] must be changed before deployment.
 */

#ifndef CONFIG_H
#define CONFIG_H

// =============================================================================
// FIRMWARE VERSION
// =============================================================================
#define FIRMWARE_VERSION_MAJOR 2
#define FIRMWARE_VERSION_MINOR 0
#define FIRMWARE_VERSION_PATCH 0
#define FIRMWARE_VERSION "2.0.0"

// =============================================================================
// DEVICE IDENTIFICATION
// =============================================================================
// Device ID is derived at runtime from the ESP32 MAC address (see globals.h)

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
#define ADMIN_PHONE "+917722087410"
#define ADMIN_PHONE_PLACEHOLDER "+1234567890"

// =============================================================================
// [REQUIRED] GSM/GPRS SETTINGS
// =============================================================================
#define GSM_PIN ""
#define APN "airtelgprs.com"
#define GPRS_USER ""
#define GPRS_PASS ""

// =============================================================================
// WIFI SETTINGS — compile-time defaults, overridden by portal config in NVS
// =============================================================================
#define WIFI_SSID "Airtel_Dantales-wifi"
#define WIFI_PASS_KEY "9823807410"
#define WIFI_CONNECT_TIMEOUT 10000UL

// =============================================================================
// MQTT BROKER SETTINGS — compile-time defaults, overridden by portal config in NVS
// =============================================================================
#define MQTT_BROKER "192.168.1.6"
//#define MQTT_BROKER "hp.iot.aquaproducts.in"
#define MQTT_PORT 1883
#define MQTT_USER "heatpump"
#define MQTT_PASS "heatpump123"
#define MQTT_BROKER_PLACEHOLDER "your-broker-ip"
#define MQTT_PASS_PLACEHOLDER "your-mqtt-password"

// =============================================================================
// TIMING INTERVALS (milliseconds)
// =============================================================================
#define SENSOR_READ_INTERVAL 2000UL    ///< Sensor polling (INCREASE FOR PRODUCTION)
#define MQTT_PUBLISH_INTERVAL 2000UL   ///< MQTT data publish (INCREASE FOR PRODUCTION)
#define ALERT_COOLDOWN 300000UL        ///< 5 minutes between same alerts
#define SMS_CHECK_INTERVAL 5000UL      ///< 5 seconds - check for SMS
#define GPRS_RETRY_INTERVAL 60000UL    ///< 1 minute between GPRS retries
#define WIFI_RETRY_INTERVAL 60000UL    ///< 1 minute between WiFi retries
#define NETWORK_TIMEOUT 60000UL        ///< 1 minute wait for network
#define WATCHDOG_TIMEOUT_S 30          ///< 30 seconds watchdog timeout

// =============================================================================
// PIN DEFINITIONS - GSM Module (SIM800C) — same as v1
// =============================================================================
#define PIN_GSM_RX 16  ///< ESP32 GPIO16 <- SIM800C TX
#define PIN_GSM_TX 17  ///< ESP32 GPIO17 -> SIM800C RX
#define GSM_BAUD 115200

// =============================================================================
// PIN DEFINITIONS - DS18B20 Temperature Sensors (OneWire bus)
// 2 sensors on shared bus: inlet + outlet
// Requires 4.7kΩ pull-up resistor from data pin to 3.3V
// =============================================================================
#define PIN_ONEWIRE 4          ///< GPIO4 - OneWire data bus
#define DS18B20_RESOLUTION 12  ///< 12-bit = 0.0625°C precision, ~750ms conversion

// =============================================================================
// PIN DEFINITIONS - PZEM-004T v3.0 (Modbus RTU over UART1)
// 3 modules on shared serial bus with different Modbus addresses
// =============================================================================
#define PIN_PZEM_RX 19        ///< GPIO19 - ESP32 RX <- PZEM TX
#define PIN_PZEM_TX 18        ///< GPIO18 - ESP32 TX -> PZEM RX
#define PZEM_BAUD 9600        ///< Modbus RTU baud rate
#define PZEM_ADDR_PHASE1 0x01 ///< Phase 1 PZEM address
#define PZEM_ADDR_PHASE2 0x02 ///< Phase 2 PZEM address
#define PZEM_ADDR_PHASE3 0x03 ///< Phase 3 PZEM address

// =============================================================================
// PIN DEFINITIONS - Status LED
// =============================================================================
#define PIN_STATUS_LED 2  ///< Built-in LED on most ESP32 dev boards

// =============================================================================
// SENSOR VALIDITY RANGES
// =============================================================================
#define TEMP_MIN_VALID -40.0f
#define TEMP_MAX_VALID 125.0f
#define VOLTAGE_MIN_VALID 0.0f
#define VOLTAGE_MAX_VALID 300.0f
#define CURRENT_MIN_VALID 0.0f
#define CURRENT_MAX_VALID 100.0f   ///< Per-phase max (PZEM supports up to 100A)
#define POWER_FACTOR_MIN_VALID 0.0f
#define POWER_FACTOR_MAX_VALID 1.0f

// =============================================================================
// ALERT THRESHOLDS (per-phase)
// =============================================================================
#define VOLTAGE_HIGH_CRITICAL 250.0f
#define VOLTAGE_HIGH_WARNING 245.0f
#define VOLTAGE_LOW_WARNING 215.0f
#define VOLTAGE_LOW_CRITICAL 210.0f
#define CURRENT_CRITICAL 15.0f
#define CURRENT_WARNING 12.0f

// =============================================================================
// DATA BUFFER SETTINGS
// =============================================================================
#define BUFFER_SIZE 100  ///< Maximum readings to store when offline

// =============================================================================
// MESSAGE BUFFER SIZES
// =============================================================================
#define SMS_BUFFER_SIZE 160      ///< Max SMS message length
#define JSON_BUFFER_SIZE 2048    ///< JSON payload buffer (larger for 3-phase)
#define STATUS_BUFFER_SIZE 256   ///< Status message buffer size

#endif  // CONFIG_H
