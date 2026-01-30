/*
 * Smart Heat Pump Monitoring System
 * ==================================
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
 * Author: Smart Heat Pump Project
 * License: MIT
 */

// ============================================================================
// INCLUDES
// ============================================================================
#include "config.h"
#include "types.h"

// TinyGSM must be configured before inclusion
#define TINY_GSM_MODEM_SIM800
#define TINY_GSM_RX_BUFFER 256

#include <TinyGsmClient.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

// Include our modules
#include "sensors.h"
#include "gsm.h"
#include "alerts.h"
#include "buffer.h"
#include "mqtt.h"

// ============================================================================
// GLOBAL OBJECTS
// ============================================================================

// GSM/Modem objects
TinyGsm modem(Serial2);
TinyGsmClient gsmClient(modem);
GSMState gsmState = GSM_UNINITIALIZED;

// MQTT client using GSM client for transport
PubSubClient mqtt(gsmClient);

// Current sensor data
SystemData currentData;

// ============================================================================
// TIMING VARIABLES
// ============================================================================
unsigned long lastSensorRead = 0;
unsigned long lastMQTTPublish = 0;
unsigned long lastSMSCheck = 0;
unsigned long lastGPRSAttempt = 0;

// ============================================================================
// STATE FLAGS
// ============================================================================
bool networkReady = false;
bool startupComplete = false;

// ============================================================================
// SETUP
// ============================================================================
void setup() {
    // Initialize Serial for debugging
    Serial.begin(115200);
    delay(1000);

    Serial.println();
    Serial.println(F("====================================="));
    Serial.println(F("  Smart Heat Pump Monitor v1.0"));
    Serial.println(F("====================================="));
    Serial.print(F("Device ID: "));
    Serial.println(DEVICE_ID);
    Serial.print(F("Simulation Mode: "));
    Serial.println(SIMULATION_MODE ? "ON" : "OFF");
    Serial.println();

    // Initialize status LED
    pinMode(PIN_STATUS_LED, OUTPUT);
    blinkLED(3, 200);  // Startup indication

    // Initialize sensors
    initSensors();

    // Initialize data buffer
    initBuffer();

    // Initialize alert system
    initAlerts();

    // Initialize GSM module
    Serial.println(F("\n--- GSM Initialization ---"));
    if (initGSM()) {
        // Wait for network registration
        if (waitForNetwork()) {
            networkReady = true;

            // Send startup notification
            String startupMsg = "Heat Pump Monitor Started\n";
            startupMsg += "Device: ";
            startupMsg += DEVICE_ID;
            startupMsg += "\nMode: ";
            startupMsg += SIMULATION_MODE ? "Simulation" : "Live";

            sendSMS(ADMIN_PHONE, startupMsg);

            // Clear any old SMS messages
            deleteAllSMS();
        }
    }

    // Set up MQTT
    mqtt.setServer(MQTT_BROKER, MQTT_PORT);
    mqtt.setCallback(mqttCallback);
    mqtt.setKeepAlive(60);
    mqtt.setBufferSize(512);

    Serial.println(F("\n--- Initialization Complete ---"));
    Serial.println(F("Starting main loop...\n"));

    startupComplete = true;

    // Take initial reading
    currentData = readAllSensors();
    printSensorData(currentData);
}

// ============================================================================
// MAIN LOOP
// ============================================================================
void loop() {
    unsigned long currentMillis = millis();

    // Blink LED to show we're alive
    static unsigned long lastBlink = 0;
    if (currentMillis - lastBlink >= 5000) {
        lastBlink = currentMillis;
        blinkLED(1, 50);
    }

    // =========================================================
    // TASK 1: Read sensors every SENSOR_READ_INTERVAL (10 sec)
    // =========================================================
    if (currentMillis - lastSensorRead >= SENSOR_READ_INTERVAL) {
        lastSensorRead = currentMillis;

        Serial.println(F("\n[MAIN] Reading sensors..."));

        // Read all sensors
        currentData = readAllSensors();

        // Print to serial for debugging
        printSensorData(currentData);

        // Check alerts and send SMS if critical
        if (networkReady) {
            checkAllAlerts(currentData);
        }

        // Buffer data for MQTT
        bufferData(currentData);
        printBufferStatus();
    }

    // =========================================================
    // TASK 2: Check for incoming SMS every SMS_CHECK_INTERVAL
    // =========================================================
    if (networkReady && (currentMillis - lastSMSCheck >= SMS_CHECK_INTERVAL)) {
        lastSMSCheck = currentMillis;

        SMSMessage msg;
        if (checkIncomingSMS(msg)) {
            handleSMSCommand(msg);
        }
    }

    // =========================================================
    // TASK 3: MQTT publish every MQTT_PUBLISH_INTERVAL (5 min)
    // =========================================================
    if (currentMillis - lastMQTTPublish >= MQTT_PUBLISH_INTERVAL) {
        lastMQTTPublish = currentMillis;

        Serial.println(F("\n[MAIN] MQTT publish cycle..."));

        // Only attempt GPRS if we have network
        if (networkReady) {
            // Connect GPRS if needed
            if (!isGPRSConnected()) {
                // Rate limit GPRS connection attempts
                if (currentMillis - lastGPRSAttempt >= GPRS_RETRY_INTERVAL) {
                    lastGPRSAttempt = currentMillis;
                    connectGPRS();
                }
            }

            // If GPRS is connected, try MQTT
            if (isGPRSConnected()) {
                if (connectMQTT()) {
                    // Publish buffered data
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
    if (mqtt.connected()) {
        mqtt.loop();
    }

    // Small delay to prevent watchdog issues
    delay(10);
}

// ============================================================================
// HANDLE SMS COMMANDS
// ============================================================================
void handleSMSCommand(const SMSMessage& msg) {
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
            // Optionally send help message
            sendSMS(msg.sender.c_str(), "Unknown command.\nValid commands: STATUS, RESET");
            break;
    }
}

// ============================================================================
// HANDLE STATUS COMMAND
// ============================================================================
void handleStatusCommand(const String& sender) {
    // Read fresh sensor data
    SystemData data = readAllSensors();

    // Format status message
    String msg = formatStatusMessage(data);

    // Add buffer status
    msg += "\n" + getBufferStatus();

    // Add alert status
    msg += "\n" + getAlertSummary();

    // Send response
    sendSMS(sender.c_str(), msg);
}

// ============================================================================
// HANDLE RESET COMMAND
// ============================================================================
void handleResetCommand() {
    // Send confirmation before reset
    sendSMS(ADMIN_PHONE, "Restarting device...");

    delay(2000);  // Give time for SMS to send

    // Restart ESP32
    ESP.restart();
}

// ============================================================================
// BLINK STATUS LED
// ============================================================================
void blinkLED(int times, int duration) {
    for (int i = 0; i < times; i++) {
        digitalWrite(PIN_STATUS_LED, HIGH);
        delay(duration);
        digitalWrite(PIN_STATUS_LED, LOW);
        if (i < times - 1) {
            delay(duration);
        }
    }
}

// ============================================================================
// WATCHDOG / ERROR RECOVERY (future enhancement)
// ============================================================================
void checkSystemHealth() {
    // Check if GSM module is responsive
    if (gsmState == GSM_ERROR) {
        Serial.println(F("[MAIN] GSM error detected, attempting recovery..."));
        if (initGSM()) {
            if (waitForNetwork()) {
                networkReady = true;
            }
        }
    }

    // Check for memory issues
    Serial.print(F("[MAIN] Free heap: "));
    Serial.println(ESP.getFreeHeap());
}
