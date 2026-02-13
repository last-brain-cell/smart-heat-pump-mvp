/**
 * @file firmware.ino
 * @brief Smart Heat Pump Monitoring System - Main Entry Point
 *
 * ESP32-based monitoring system for heat pumps in remote locations.
 * Uses GSM cellular (SIM800C) for communication - no WiFi required.
 *
 * Features:
 * - Sensor monitoring (temperature, voltage, current, pressure)
 * - SMS alerts for critical conditions
 * - SMS commands (STATUS, RESET)
 * - MQTT data publishing over GPRS
 * - Local data buffering when offline
 * - Watchdog timer for automatic recovery
 *
 * Hardware:
 * - ESP32 WROOM
 * - SIM800C GSM Module
 * - 4x 10K NTC Thermistors
 * - ZMPT101B AC Voltage Sensor
 * - ACS712-20A Current Sensor
 * - 2x Pressure Transducers (optional)
 *
 * Required Libraries:
 * - TinyGSM (by Volodymyr Shymanskyy)
 * - PubSubClient (by Nick O'Leary)
 * - ArduinoJson (by Benoit Blanchon)
 *
 * @author Smart Heat Pump Project
 * @version 1.0.0
 * @license MIT
 */

// =============================================================================
// INCLUDES
// =============================================================================

#include <esp_task_wdt.h>
#include "config.h"
#include "src/types.h"
#include "src/globals.h"
#include "src/sensors.h"
#include "src/gsm.h"
#include "src/alerts.h"
#include "src/buffer.h"
#include "src/mqtt.h"
#include "src/provision.h"
#include "src/dashboard.h"

// =============================================================================
// GLOBAL OBJECT DEFINITIONS
// =============================================================================

LogCapture Log(Serial);
TinyGsm modem(Serial2);
TinyGsmClient gsmClient(modem);
GSMState gsmState = GSM_UNINITIALIZED;
PubSubClient mqtt(gsmClient);
SystemData currentData;
bool networkReady = false;
bool startupComplete = false;
WiFiClient wifiClient;
ConnectionType activeConnection = CONN_NONE;
RuntimeConfig runtimeCfg;

// =============================================================================
// TIMING STATE
// =============================================================================

static unsigned long lastSensorRead = 0;
static unsigned long lastMQTTPublish = 0;
static unsigned long lastSMSCheck = 0;
static unsigned long lastGPRSAttempt = 0;
static unsigned long lastWiFiAttempt = 0;
static unsigned long lastBlink = 0;

// =============================================================================
// FUNCTION DECLARATIONS
// =============================================================================

static void validateConfiguration();
static void handleSMSCommand(const SMSMessage& msg);
static void handleStatusCommand(const String& sender);
static void handleResetCommand();
static void blinkLED(int times, int duration);
static void printStartupBanner();
static bool connectWiFi();
static bool isWiFiConnected();
static void ensureMQTTTransport(unsigned long currentMillis);
static void handleWiFiResetCommand(const String& sender);

// =============================================================================
// CONFIGURATION VALIDATION
// =============================================================================

/**
 * @brief Check for placeholder configuration values
 * Warns on serial if required configuration hasn't been changed
 */
static void validateConfiguration() {
    Log.println(F("\n--- Configuration Validation ---"));
    bool hasWarnings = false;

    if (strcmp(ADMIN_PHONE, ADMIN_PHONE_PLACEHOLDER) == 0) {
        Log.println(F("WARNING: ADMIN_PHONE is still placeholder value!"));
        hasWarnings = true;
    }

    if (strcmp(MQTT_BROKER, MQTT_BROKER_PLACEHOLDER) == 0) {
        Log.println(F("WARNING: MQTT_BROKER is still placeholder value!"));
        hasWarnings = true;
    }

    if (strcmp(MQTT_PASS, MQTT_PASS_PLACEHOLDER) == 0) {
        Log.println(F("WARNING: MQTT_PASS is still placeholder value!"));
        hasWarnings = true;
    }

    if (hasWarnings) {
        Log.println(F("Please update config.h before deployment."));
    } else {
        Log.println(F("Configuration OK"));
    }
}

// =============================================================================
// STARTUP BANNER
// =============================================================================

static void printStartupBanner() {
    Log.println();
    Log.println(F("====================================="));
    Log.println(F("  Smart Heat Pump Monitor"));
    Log.print(F("  Version: "));
    Log.println(FIRMWARE_VERSION);
    Log.println(F("====================================="));
    Log.print(F("Device ID: "));
    Log.println(DEVICE_ID);
    Log.print(F("Mode: "));
    Log.println(SIMULATION_MODE ? "Simulation" : "Live");
    Log.print(F("Free Heap: "));
    Log.print(ESP.getFreeHeap());
    Log.println(F(" bytes"));
}

// =============================================================================
// SETUP
// =============================================================================

void setup() {
    // Initialize Serial for debugging
    Log.begin(115200);
    delay(1000);

    printStartupBanner();
    validateConfiguration();

    // Initialize watchdog timer
    Log.println(F("\n--- Watchdog Timer ---"));
#if ESP_ARDUINO_VERSION >= ESP_ARDUINO_VERSION_VAL(3, 0, 0)
    // ESP32 Arduino Core 3.x uses new config struct API
    esp_task_wdt_config_t wdtConfig = {
        .timeout_ms = WATCHDOG_TIMEOUT_S * 1000,
        .idle_core_mask = (1 << portNUM_PROCESSORS) - 1,
        .trigger_panic = true
    };
    esp_task_wdt_reconfigure(&wdtConfig);
#else
    // ESP32 Arduino Core 2.x uses legacy API
    esp_task_wdt_init(WATCHDOG_TIMEOUT_S, true);
#endif
    esp_task_wdt_add(NULL);  // Add current task to watchdog
    Log.print(F("Watchdog enabled: "));
    Log.print(WATCHDOG_TIMEOUT_S);
    Log.println(F("s timeout"));

    // Initialize status LED
    pinMode(PIN_STATUS_LED, OUTPUT);
    blinkLED(3, 200);  // Startup indication

    // Initialize subsystems
    initSensors();
    initBuffer();
    initAlerts();

    // Initialize GSM module
    esp_task_wdt_reset();  // Reset watchdog before long-blocking GSM init
    Log.println(F("\n--- GSM Initialization ---"));
    if (initGSM()) {
        if (waitForNetwork()) {
            networkReady = true;

            // Send startup notification
            char startupMsg[SMS_BUFFER_SIZE];
            snprintf(startupMsg, sizeof(startupMsg),
                "Heat Pump Monitor Started\n"
                "Device: %s\n"
                "Version: %s\n"
                "Mode: %s",
                DEVICE_ID,
                FIRMWARE_VERSION,
                SIMULATION_MODE ? "Simulation" : "Live"
            );

            sendSMS(ADMIN_PHONE, startupMsg);
            deleteAllSMS();
        }
    }

    // Load runtime config from NVS (falls back to config.h defaults)
    esp_task_wdt_reset();
    Log.println(F("\n--- Provisioning ---"));
    loadConfig(runtimeCfg);

    if (!isProvisioned()) {
        startProvisioningPortal(runtimeCfg);
    }

    // Attempt WiFi connection
    esp_task_wdt_reset();  // Reset watchdog before long-blocking WiFi init
    Log.println(F("\n--- WiFi Initialization ---"));
    if (connectWiFi()) {
        activeConnection = CONN_WIFI;
        mqtt.setClient(wifiClient);
        Log.println(F("[WIFI] Will use WiFi for MQTT"));
        initDashboard();
    } else {
        Log.println(F("[WIFI] Not available, re-launching portal for credential fix"));
        startProvisioningPortal(runtimeCfg);
        esp_task_wdt_reset();
        if (connectWiFi()) {
            activeConnection = CONN_WIFI;
            mqtt.setClient(wifiClient);
            Log.println(F("[WIFI] Connected after portal, will use WiFi for MQTT"));
            initDashboard();
        } else {
            Log.println(F("[WIFI] Still not available, will use GPRS for MQTT"));
        }
    }

    // Configure MQTT
    mqtt.setServer(runtimeCfg.mqttHost, runtimeCfg.mqttPort);
    mqtt.setCallback(mqttCallback);
    mqtt.setKeepAlive(60);
    mqtt.setBufferSize(JSON_BUFFER_SIZE);

    Log.println(F("\n--- Initialization Complete ---"));
    Log.println(F("Starting main loop...\n"));

    startupComplete = true;

    // Take initial reading
    esp_task_wdt_reset();
    currentData = readAllSensors();
    printSensorData(currentData);
}

// =============================================================================
// MAIN LOOP
// =============================================================================

void loop() {
    unsigned long currentMillis = millis();

    // Feed watchdog
    esp_task_wdt_reset();

    // Heartbeat LED blink
    if (currentMillis - lastBlink >= 5000) {
        lastBlink = currentMillis;
        blinkLED(1, 50);
    }

    // =========================================================
    // TASK 1: Read sensors (every SENSOR_READ_INTERVAL)
    // =========================================================
    if (currentMillis - lastSensorRead >= SENSOR_READ_INTERVAL) {
        lastSensorRead = currentMillis;

        Log.println(F("\n[MAIN] Reading sensors..."));
        currentData = readAllSensors();
        printSensorData(currentData);

        if (networkReady) {
            checkAllAlerts(currentData);
        }

        bufferData(currentData);
        printBufferStatus();
    }

    // =========================================================
    // TASK 2: Check for SMS commands (every SMS_CHECK_INTERVAL)
    // =========================================================
    if (networkReady && (currentMillis - lastSMSCheck >= SMS_CHECK_INTERVAL)) {
        lastSMSCheck = currentMillis;

        SMSMessage msg;
        if (checkIncomingSMS(msg)) {
            handleSMSCommand(msg);
        }
    }

    // =========================================================
    // TASK 3: MQTT publish (every MQTT_PUBLISH_INTERVAL)
    // =========================================================
    if (currentMillis - lastMQTTPublish >= MQTT_PUBLISH_INTERVAL) {
        lastMQTTPublish = currentMillis;

        Log.println(F("\n[MAIN] MQTT publish cycle..."));

        ensureMQTTTransport(currentMillis);

        if (activeConnection != CONN_NONE) {
            if (connectMQTT()) {
                publishBufferedData();
            }
        } else {
            Log.println(F("[MAIN] No transport available - skipping MQTT"));
        }
    }

    // =========================================================
    // TASK 4: Maintain MQTT connection
    // =========================================================
    if (activeConnection == CONN_WIFI && !isWiFiConnected()) {
        Log.println(F("[MAIN] WiFi dropped, resetting MQTT transport"));
        disconnectMQTT();
        stopDashboard();
        activeConnection = CONN_NONE;
    } else if (activeConnection == CONN_GPRS && !isGPRSConnected()) {
        Log.println(F("[MAIN] GPRS dropped, resetting MQTT transport"));
        disconnectMQTT();
        activeConnection = CONN_NONE;
    }
    mqttLoop();
    handleDashboard();

    // Small delay to prevent watchdog issues and reduce power
    delay(10);
}

// =============================================================================
// WIFI HELPERS
// =============================================================================

static bool connectWiFi() {
    Log.print(F("[WIFI] Connecting to "));
    Log.println(runtimeCfg.wifiSSID);

    WiFi.mode(WIFI_STA);
    WiFi.begin(runtimeCfg.wifiSSID, runtimeCfg.wifiPass);

    unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - start < WIFI_CONNECT_TIMEOUT) {
        delay(250);
        Log.print(F("."));
    }
    Log.println();

    if (WiFi.status() == WL_CONNECTED) {
        Log.print(F("[WIFI] Connected, IP: "));
        Log.println(WiFi.localIP());
        return true;
    }

    Log.println(F("[WIFI] Connection failed"));
    WiFi.disconnect(true);
    return false;
}

static bool isWiFiConnected() {
    return WiFi.status() == WL_CONNECTED;
}

/**
 * @brief Ensure an MQTT transport is active (WiFi preferred, GPRS fallback)
 *
 * Tries WiFi first. If WiFi is unavailable, falls back to GPRS.
 * Calls mqtt.setClient() with the appropriate client and sets activeConnection.
 */
static void ensureMQTTTransport(unsigned long currentMillis) {
    // Try WiFi first (always preferred)
    if (isWiFiConnected()) {
        if (activeConnection != CONN_WIFI) {
            Log.println(F("[MAIN] Switching MQTT transport to WiFi"));
            disconnectMQTT();
            mqtt.setClient(wifiClient);
            activeConnection = CONN_WIFI;
        }
        return;
    }

    // WiFi not connected — attempt reconnect if retry interval passed
    if (currentMillis - lastWiFiAttempt >= WIFI_RETRY_INTERVAL) {
        lastWiFiAttempt = currentMillis;
        if (connectWiFi()) {
            Log.println(F("[MAIN] Switching MQTT transport to WiFi"));
            disconnectMQTT();
            mqtt.setClient(wifiClient);
            activeConnection = CONN_WIFI;
            initDashboard();
            return;
        }
    }

    // Fall back to GPRS
    if (isGPRSConnected()) {
        if (activeConnection != CONN_GPRS) {
            Log.println(F("[MAIN] Switching MQTT transport to GPRS"));
            disconnectMQTT();
            mqtt.setClient(gsmClient);
            activeConnection = CONN_GPRS;
        }
        return;
    }

    // GPRS not connected — attempt reconnect if retry interval passed
    if (currentMillis - lastGPRSAttempt >= GPRS_RETRY_INTERVAL) {
        lastGPRSAttempt = currentMillis;
        if (connectGPRS()) {
            Log.println(F("[MAIN] Switching MQTT transport to GPRS"));
            disconnectMQTT();
            mqtt.setClient(gsmClient);
            activeConnection = CONN_GPRS;
            return;
        }
    }

    // Neither transport available
    activeConnection = CONN_NONE;
}

// =============================================================================
// SMS COMMAND HANDLING
// =============================================================================

static void handleSMSCommand(const SMSMessage& msg) {
    SMSCommand cmd = parseSMSCommand(msg.content);

    Log.print(F("[MAIN] SMS command from "));
    Log.print(msg.sender);
    Log.print(F(": "));

    switch (cmd) {
        case SMS_CMD_STATUS:
            Log.println(F("STATUS"));
            handleStatusCommand(msg.sender);
            break;

        case SMS_CMD_RESET:
            Log.println(F("RESET"));
            handleResetCommand();
            break;

        case SMS_CMD_WIFI_RESET:
            Log.println(F("WIFI RESET"));
            handleWiFiResetCommand(msg.sender);
            break;

        default:
            Log.println(F("UNKNOWN"));
            sendSMS(msg.sender.c_str(), "Unknown command.\nValid: STATUS, RESET, WIFI RESET");
            break;
    }
}

static void handleStatusCommand(const String& sender) {
    SystemData data = readAllSensors();

    char statusMsg[SMS_BUFFER_SIZE];
    char bufferStatus[32];
    char alertSummary[64];

    formatStatusMessage(data, statusMsg, sizeof(statusMsg));
    getBufferStatus(bufferStatus, sizeof(bufferStatus));
    getAlertSummary(alertSummary, sizeof(alertSummary));

    // Build combined message
    char fullMsg[SMS_BUFFER_SIZE];
    snprintf(fullMsg, sizeof(fullMsg), "%s\n%s\n%s",
             statusMsg, bufferStatus, alertSummary);

    sendSMS(sender.c_str(), fullMsg);
}

static void handleResetCommand() {
    sendSMS(ADMIN_PHONE, "Restarting device...");
    delay(2000);  // Allow SMS to send
    ESP.restart();
}

static void handleWiFiResetCommand(const String& sender) {
    sendSMS(sender.c_str(), "WiFi config cleared.\nRestarting into setup portal...");
    clearConfig();
    delay(2000);  // Allow SMS to send
    ESP.restart();
}

// =============================================================================
// UTILITY FUNCTIONS
// =============================================================================

static void blinkLED(int times, int duration) {
    for (int i = 0; i < times; i++) {
        digitalWrite(PIN_STATUS_LED, HIGH);
        delay(duration);
        digitalWrite(PIN_STATUS_LED, LOW);
        if (i < times - 1) {
            delay(duration);
        }
    }
}
