/**
 * @file provision.h
 * @brief WiFi provisioning portal for runtime configuration
 *
 * Provides a captive portal web server for configuring WiFi and MQTT
 * settings at runtime, stored in NVS flash. On first boot (or after
 * SMS "WIFI RESET"), the ESP32 starts as an AP with a config form.
 */

#ifndef PROVISION_H
#define PROVISION_H

#include <Arduino.h>
#include "../config.h"

// =============================================================================
// PORTAL CONFIGURATION
// =============================================================================

#define PROVISION_AP_SSID    "HeatPump-Setup"
#define PROVISION_TIMEOUT_MS 180000UL   ///< 3 minutes portal timeout
#define PROVISION_NVS_NS     "hpcfg"    ///< NVS namespace for config

// =============================================================================
// RUNTIME CONFIGURATION
// =============================================================================

/**
 * @brief Runtime configuration loaded from NVS (or config.h defaults)
 */
struct RuntimeConfig {
    char wifiSSID[64];
    char wifiPass[64];
    char mqttHost[64];
    uint16_t mqttPort;
    char mqttUser[32];
    char mqttPass[64];

    RuntimeConfig() {
        strncpy(wifiSSID, WIFI_SSID, sizeof(wifiSSID) - 1);
        wifiSSID[sizeof(wifiSSID) - 1] = '\0';
        strncpy(wifiPass, WIFI_PASS_KEY, sizeof(wifiPass) - 1);
        wifiPass[sizeof(wifiPass) - 1] = '\0';
        strncpy(mqttHost, MQTT_BROKER, sizeof(mqttHost) - 1);
        mqttHost[sizeof(mqttHost) - 1] = '\0';
        mqttPort = MQTT_PORT;
        strncpy(mqttUser, MQTT_USER, sizeof(mqttUser) - 1);
        mqttUser[sizeof(mqttUser) - 1] = '\0';
        strncpy(mqttPass, MQTT_PASS, sizeof(mqttPass) - 1);
        mqttPass[sizeof(mqttPass) - 1] = '\0';
    }
};

// =============================================================================
// FUNCTION DECLARATIONS
// =============================================================================

/**
 * @brief Load configuration from NVS into cfg. Falls back to config.h defaults.
 */
void loadConfig(RuntimeConfig& cfg);

/**
 * @brief Save current configuration to NVS.
 */
void saveConfig(const RuntimeConfig& cfg);

/**
 * @brief Clear all stored configuration from NVS.
 */
void clearConfig();

/**
 * @brief Check if NVS contains a saved configuration.
 * @return true if a config has been saved via the portal or saveConfig()
 */
bool isProvisioned();

/**
 * @brief Start the AP-mode provisioning portal.
 *
 * Blocks until the user submits the form or PROVISION_TIMEOUT_MS elapses.
 * On submit, saves to NVS and updates cfg in place.
 * On timeout, cfg retains its current values (config.h defaults).
 */
void startProvisioningPortal(RuntimeConfig& cfg);

#endif // PROVISION_H
