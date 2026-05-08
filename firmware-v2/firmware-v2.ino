/**
 * @file firmware-v2.ino
 * @brief Smart Heat Pump Monitoring System V2 - Main Entry Point
 *
 * V2 Hardware:
 *   - 3x PZEM-004T v3.0 (3-phase power monitoring via Modbus RTU)
 *   - 2x DS18B20 (inlet/outlet temperature via OneWire)
 *   - SIM800C GSM Module (SMS alerts + GPRS fallback)
 *
 * Required Libraries:
 *   - TinyGSM (by Volodymyr Shymanskyy)
 *   - PubSubClient (by Nick O'Leary)
 *   - ArduinoJson (by Benoit Blanchon)
 *   - PZEM004Tv30 (by Jakub Mandula)
 *   - OneWire (by Jim Studt)
 *   - DallasTemperature (by Miles Burton)
 *
 * @version 2.0.0
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
char deviceId[12] = "";
char mqttTopicBase[32] = "";
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
    Log.println(F("  Smart Heat Pump Monitor V2"));
    Log.println(F("  3-Phase PZEM + DS18B20"));
    Log.print(F("  Version: "));
    Log.println(FIRMWARE_VERSION);
    Log.println(F("====================================="));
    Log.print(F("Device ID: "));
    Log.println(deviceId);
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
    Log.begin(115200);
    delay(1000);

    // Derive unique device ID from ESP32 MAC address
    uint64_t mac = ESP.getEfuseMac();
    snprintf(deviceId, sizeof(deviceId), "HP-%08X", (uint32_t)(mac >> 16));
    snprintf(mqttTopicBase, sizeof(mqttTopicBase), "heatpump/%s", deviceId);

    printStartupBanner();
    validateConfiguration();

    // Initialize watchdog timer
    Log.println(F("\n--- Watchdog Timer ---"));
#if ESP_ARDUINO_VERSION >= ESP_ARDUINO_VERSION_VAL(3, 0, 0)
    esp_task_wdt_config_t wdtConfig = {
        .timeout_ms = WATCHDOG_TIMEOUT_S * 1000,
        .idle_core_mask = (1 << portNUM_PROCESSORS) - 1,
        .trigger_panic = true
    };
    esp_task_wdt_reconfigure(&wdtConfig);
#else
    esp_task_wdt_init(WATCHDOG_TIMEOUT_S, true);
#endif
    esp_task_wdt_add(NULL);
    Log.print(F("Watchdog enabled: "));
    Log.print(WATCHDOG_TIMEOUT_S);
    Log.println(F("s timeout"));

    // Initialize status LED
    pinMode(PIN_STATUS_LED, OUTPUT);
    blinkLED(3, 200);

    // Initialize V2 subsystems
    initSensors();
    initBuffer();
    initAlerts();

    // Load runtime config from NVS
    esp_task_wdt_reset();
    Log.println(F("\n--- Provisioning ---"));
    loadConfig(runtimeCfg);

    if (!isProvisioned()) {
        startProvisioningPortal(runtimeCfg);
    }

    // Attempt WiFi connection
    esp_task_wdt_reset();
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

    Log.println(F("\n--- V2 Initialization Complete ---"));
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

    esp_task_wdt_reset();

    // Heartbeat LED
    if (currentMillis - lastBlink >= 5000) {
        lastBlink = currentMillis;
        blinkLED(1, 50);
    }

    // TASK 1: Read sensors
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

    // TASK 2: Check for SMS commands
    if (networkReady && (currentMillis - lastSMSCheck >= SMS_CHECK_INTERVAL)) {
        lastSMSCheck = currentMillis;

        SMSMessage msg;
        if (checkIncomingSMS(msg)) {
            handleSMSCommand(msg);
        }
    }

    // TASK 3: MQTT publish
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

    // TASK 4: Maintain MQTT connection
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

static void ensureMQTTTransport(unsigned long currentMillis) {
    if (isWiFiConnected()) {
        if (activeConnection != CONN_WIFI) {
            Log.println(F("[MAIN] Switching MQTT transport to WiFi"));
            disconnectMQTT();
            mqtt.setClient(wifiClient);
            activeConnection = CONN_WIFI;
        }
        return;
    }

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

    if (isGPRSConnected()) {
        if (activeConnection != CONN_GPRS) {
            Log.println(F("[MAIN] Switching MQTT transport to GPRS"));
            disconnectMQTT();
            mqtt.setClient(gsmClient);
            activeConnection = CONN_GPRS;
        }
        return;
    }

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

    // Build V2 status message with 3-phase data
    char fullMsg[SMS_BUFFER_SIZE];
    char alertSummary[64];
    getAlertSummary(alertSummary, sizeof(alertSummary));

    // Calculate totals for SMS (SMS is limited to 160 chars)
    float totalPower = 0;
    for (int i = 0; i < 3; i++) {
        if (data.phase[i].valid) totalPower += data.phase[i].power;
    }

    snprintf(fullMsg, sizeof(fullMsg),
        "V2 %s\n"
        "In:%.1fC Out:%.1fC\n"
        "P1:%.0fV P2:%.0fV P3:%.0fV\n"
        "TotW:%.0f\n"
        "%s",
        deviceId,
        data.tempInlet.value, data.tempOutlet.value,
        data.phase[0].voltage.value, data.phase[1].voltage.value, data.phase[2].voltage.value,
        totalPower,
        alertSummary
    );

    sendSMS(sender.c_str(), fullMsg);
}

static void handleResetCommand() {
    sendSMS(ADMIN_PHONE, "Restarting device...");
    delay(2000);
    ESP.restart();
}

static void handleWiFiResetCommand(const String& sender) {
    sendSMS(sender.c_str(), "WiFi config cleared.\nRestarting into setup portal...");
    clearConfig();
    delay(2000);
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
