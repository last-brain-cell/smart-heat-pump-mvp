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

// =============================================================================
// GLOBAL OBJECT DEFINITIONS
// =============================================================================

TinyGsm modem(Serial2);
TinyGsmClient gsmClient(modem);
GSMState gsmState = GSM_UNINITIALIZED;
PubSubClient mqtt(gsmClient);
SystemData currentData;
bool networkReady = false;
bool startupComplete = false;

// =============================================================================
// TIMING STATE
// =============================================================================

static unsigned long lastSensorRead = 0;
static unsigned long lastMQTTPublish = 0;
static unsigned long lastSMSCheck = 0;
static unsigned long lastGPRSAttempt = 0;
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

// =============================================================================
// CONFIGURATION VALIDATION
// =============================================================================

/**
 * @brief Check for placeholder configuration values
 * Warns on serial if required configuration hasn't been changed
 */
static void validateConfiguration() {
    Serial.println(F("\n--- Configuration Validation ---"));
    bool hasWarnings = false;

    if (strcmp(ADMIN_PHONE, ADMIN_PHONE_PLACEHOLDER) == 0) {
        Serial.println(F("WARNING: ADMIN_PHONE is still placeholder value!"));
        hasWarnings = true;
    }

    if (strcmp(MQTT_BROKER, MQTT_BROKER_PLACEHOLDER) == 0) {
        Serial.println(F("WARNING: MQTT_BROKER is still placeholder value!"));
        hasWarnings = true;
    }

    if (strcmp(MQTT_PASS, MQTT_PASS_PLACEHOLDER) == 0) {
        Serial.println(F("WARNING: MQTT_PASS is still placeholder value!"));
        hasWarnings = true;
    }

    if (hasWarnings) {
        Serial.println(F("Please update config.h before deployment."));
    } else {
        Serial.println(F("Configuration OK"));
    }
}

// =============================================================================
// STARTUP BANNER
// =============================================================================

static void printStartupBanner() {
    Serial.println();
    Serial.println(F("====================================="));
    Serial.println(F("  Smart Heat Pump Monitor"));
    Serial.print(F("  Version: "));
    Serial.println(FIRMWARE_VERSION);
    Serial.println(F("====================================="));
    Serial.print(F("Device ID: "));
    Serial.println(DEVICE_ID);
    Serial.print(F("Mode: "));
    Serial.println(SIMULATION_MODE ? "Simulation" : "Live");
    Serial.print(F("Free Heap: "));
    Serial.print(ESP.getFreeHeap());
    Serial.println(F(" bytes"));
}

// =============================================================================
// SETUP
// =============================================================================

void setup() {
    // Initialize Serial for debugging
    Serial.begin(115200);
    delay(1000);

    printStartupBanner();
    validateConfiguration();

    // Initialize watchdog timer
    Serial.println(F("\n--- Watchdog Timer ---"));
#if ESP_ARDUINO_VERSION >= ESP_ARDUINO_VERSION_VAL(3, 0, 0)
    // ESP32 Arduino Core 3.x uses new config struct API
    esp_task_wdt_config_t wdtConfig = {
        .timeout_ms = WATCHDOG_TIMEOUT_S * 1000,
        .idle_core_mask = (1 << portNUM_PROCESSORS) - 1,
        .trigger_panic = true
    };
    esp_task_wdt_init(&wdtConfig);
#else
    // ESP32 Arduino Core 2.x uses legacy API
    esp_task_wdt_init(WATCHDOG_TIMEOUT_S, true);
#endif
    esp_task_wdt_add(NULL);  // Add current task to watchdog
    Serial.print(F("Watchdog enabled: "));
    Serial.print(WATCHDOG_TIMEOUT_S);
    Serial.println(F("s timeout"));

    // Initialize status LED
    pinMode(PIN_STATUS_LED, OUTPUT);
    blinkLED(3, 200);  // Startup indication

    // Initialize subsystems
    initSensors();
    initBuffer();
    initAlerts();

    // Initialize GSM module
    Serial.println(F("\n--- GSM Initialization ---"));
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

    // Configure MQTT
    mqtt.setServer(MQTT_BROKER, MQTT_PORT);
    mqtt.setCallback(mqttCallback);
    mqtt.setKeepAlive(60);
    mqtt.setBufferSize(JSON_BUFFER_SIZE);

    Serial.println(F("\n--- Initialization Complete ---"));
    Serial.println(F("Starting main loop...\n"));

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

        Serial.println(F("\n[MAIN] Reading sensors..."));
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

        Serial.println(F("\n[MAIN] MQTT publish cycle..."));

        if (networkReady) {
            if (!isGPRSConnected()) {
                if (currentMillis - lastGPRSAttempt >= GPRS_RETRY_INTERVAL) {
                    lastGPRSAttempt = currentMillis;
                    connectGPRS();
                }
            }

            if (isGPRSConnected()) {
                if (connectMQTT()) {
                    publishBufferedData();
                }
            }
        } else {
            Serial.println(F("[MAIN] No network - skipping MQTT"));
        }
    }

    // =========================================================
    // TASK 4: Maintain MQTT connection
    // =========================================================
    mqttLoop();

    // Small delay to prevent watchdog issues and reduce power
    delay(10);
}

// =============================================================================
// SMS COMMAND HANDLING
// =============================================================================

static void handleSMSCommand(const SMSMessage& msg) {
    SMSCommand cmd = parseSMSCommand(msg.content);

    Serial.print(F("[MAIN] SMS command from "));
    Serial.print(msg.sender);
    Serial.print(F(": "));

    switch (cmd) {
        case SMS_CMD_STATUS:
            Serial.println(F("STATUS"));
            handleStatusCommand(msg.sender);
            break;

        case SMS_CMD_RESET:
            Serial.println(F("RESET"));
            handleResetCommand();
            break;

        default:
            Serial.println(F("UNKNOWN"));
            sendSMS(msg.sender.c_str(), "Unknown command.\nValid: STATUS, RESET");
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
