/*
 * mqtt.h - MQTT Publishing Module
 *
 * Handles MQTT connection and publishing sensor data over GPRS.
 * Uses PubSubClient library with TinyGSM client.
 */

#ifndef MQTT_H
#define MQTT_H

#include <Arduino.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include "config.h"
#include "types.h"
#include "buffer.h"

// ============================================================================
// EXTERNAL DECLARATIONS
// ============================================================================
extern TinyGsmClient gsmClient;
extern PubSubClient mqtt;

// External functions from gsm.h
extern bool connectGPRS();
extern bool isGPRSConnected();

// External functions from buffer.h
extern bool bufferHasData();
extern SystemData* getNextBufferedData();
extern void markDataPublished();
extern uint8_t bufferCount();

// ============================================================================
// FUNCTION DECLARATIONS
// ============================================================================
bool connectMQTT();
void disconnectMQTT();
bool isMQTTConnected();
bool publishSensorData(const SystemData& data);
bool publishBufferedData();
bool publishStatus(bool online);
void mqttCallback(char* topic, byte* payload, unsigned int length);
String buildJsonPayload(const SystemData& data);

// ============================================================================
// CONNECT TO MQTT BROKER
// ============================================================================
bool connectMQTT() {
    // Ensure GPRS is connected first
    if (!isGPRSConnected()) {
        Serial.println(F("[MQTT] No GPRS connection"));
        return false;
    }

    if (mqtt.connected()) {
        Serial.println(F("[MQTT] Already connected"));
        return true;
    }

    Serial.print(F("[MQTT] Connecting to "));
    Serial.print(MQTT_BROKER);
    Serial.print(F(":"));
    Serial.println(MQTT_PORT);

    // Set server (may already be set, but safe to call again)
    mqtt.setServer(MQTT_BROKER, MQTT_PORT);
    mqtt.setCallback(mqttCallback);

    // Attempt connection with last will message
    String willTopic = String(MQTT_TOPIC_BASE) + "/status/online";
    String willMessage = "false";

    bool connected = mqtt.connect(
        MQTT_CLIENT_ID,
        MQTT_USER,
        MQTT_PASS,
        willTopic.c_str(),
        1,      // QoS
        true,   // Retain
        willMessage.c_str()
    );

    if (connected) {
        Serial.println(F("[MQTT] Connected!"));

        // Publish online status
        publishStatus(true);

        // Subscribe to command topic (for future remote commands)
        String cmdTopic = String(MQTT_TOPIC_BASE) + "/commands";
        mqtt.subscribe(cmdTopic.c_str());

        return true;
    } else {
        Serial.print(F("[MQTT] Connection failed, rc="));
        Serial.println(mqtt.state());
        return false;
    }
}

// ============================================================================
// DISCONNECT FROM MQTT
// ============================================================================
void disconnectMQTT() {
    if (mqtt.connected()) {
        publishStatus(false);
        mqtt.disconnect();
        Serial.println(F("[MQTT] Disconnected"));
    }
}

// ============================================================================
// CHECK IF MQTT IS CONNECTED
// ============================================================================
bool isMQTTConnected() {
    return mqtt.connected();
}

// ============================================================================
// PUBLISH ONLINE/OFFLINE STATUS
// ============================================================================
bool publishStatus(bool online) {
    String topic = String(MQTT_TOPIC_BASE) + "/status/online";
    String payload = online ? "true" : "false";

    return mqtt.publish(topic.c_str(), payload.c_str(), true);  // Retained
}

// ============================================================================
// BUILD JSON PAYLOAD FROM SENSOR DATA
// ============================================================================
String buildJsonPayload(const SystemData& data) {
    StaticJsonDocument<512> doc;

    doc["device"] = DEVICE_ID;
    doc["timestamp"] = data.readingTime;

    // Temperature object
    JsonObject temps = doc.createNestedObject("temperature");
    temps["inlet"] = round(data.tempInlet.value * 10) / 10.0;
    temps["outlet"] = round(data.tempOutlet.value * 10) / 10.0;
    temps["ambient"] = round(data.tempAmbient.value * 10) / 10.0;
    temps["compressor"] = round(data.tempCompressor.value * 10) / 10.0;

    // Electrical object
    JsonObject elec = doc.createNestedObject("electrical");
    elec["voltage"] = round(data.voltage.value * 10) / 10.0;
    elec["current"] = round(data.current.value * 100) / 100.0;
    elec["power"] = round(data.power);

    // Pressure object
    JsonObject pressure = doc.createNestedObject("pressure");
    pressure["high"] = round(data.pressureHigh.value);
    pressure["low"] = round(data.pressureLow.value);

    // Status object
    JsonObject status = doc.createNestedObject("status");
    status["compressor"] = data.compressorRunning;
    status["fan"] = data.fanRunning;
    status["defrost"] = data.defrostActive;

    // Alerts object (any active alerts)
    JsonObject alerts = doc.createNestedObject("alerts");
    alerts["voltage"] = data.voltage.alertLevel;
    alerts["compressor_temp"] = data.tempCompressor.alertLevel;
    alerts["pressure_high"] = data.pressureHigh.alertLevel;
    alerts["pressure_low"] = data.pressureLow.alertLevel;
    alerts["current"] = data.current.alertLevel;

    String output;
    serializeJson(doc, output);
    return output;
}

// ============================================================================
// PUBLISH SINGLE SENSOR DATA READING
// ============================================================================
bool publishSensorData(const SystemData& data) {
    if (!mqtt.connected()) {
        Serial.println(F("[MQTT] Not connected, cannot publish"));
        return false;
    }

    String topic = String(MQTT_TOPIC_BASE) + "/data";
    String payload = buildJsonPayload(data);

    Serial.print(F("[MQTT] Publishing to "));
    Serial.println(topic);

    bool success = mqtt.publish(topic.c_str(), payload.c_str());

    if (success) {
        Serial.println(F("[MQTT] Published successfully"));
    } else {
        Serial.println(F("[MQTT] Publish failed!"));
    }

    return success;
}

// ============================================================================
// PUBLISH ALL BUFFERED DATA
// Returns number of readings successfully published
// ============================================================================
bool publishBufferedData() {
    if (!mqtt.connected()) {
        Serial.println(F("[MQTT] Not connected, cannot publish buffer"));
        return false;
    }

    int count = bufferCount();
    if (count == 0) {
        Serial.println(F("[MQTT] Buffer empty, nothing to publish"));
        return true;
    }

    Serial.print(F("[MQTT] Publishing "));
    Serial.print(count);
    Serial.println(F(" buffered readings..."));

    int published = 0;
    int failed = 0;

    while (bufferHasData()) {
        SystemData* data = getNextBufferedData();
        if (data == nullptr) break;

        if (publishSensorData(*data)) {
            markDataPublished();
            published++;
        } else {
            failed++;
            // Stop on first failure to preserve order
            break;
        }

        // Process MQTT loop to prevent timeout
        mqtt.loop();

        // Small delay between publishes
        delay(100);
    }

    Serial.print(F("[MQTT] Published: "));
    Serial.print(published);
    Serial.print(F(", Failed: "));
    Serial.println(failed);

    return (failed == 0);
}

// ============================================================================
// PUBLISH INDIVIDUAL SENSOR VALUES TO SEPARATE TOPICS
// Alternative publishing method for real-time dashboards
// ============================================================================
bool publishIndividualValues(const SystemData& data) {
    if (!mqtt.connected()) return false;

    bool allSuccess = true;
    String base = String(MQTT_TOPIC_BASE) + "/sensors/";

    // Temperatures
    allSuccess &= mqtt.publish((base + "temperature/inlet").c_str(),
                               String(data.tempInlet.value, 1).c_str());
    allSuccess &= mqtt.publish((base + "temperature/outlet").c_str(),
                               String(data.tempOutlet.value, 1).c_str());
    allSuccess &= mqtt.publish((base + "temperature/ambient").c_str(),
                               String(data.tempAmbient.value, 1).c_str());
    allSuccess &= mqtt.publish((base + "temperature/compressor").c_str(),
                               String(data.tempCompressor.value, 1).c_str());

    // Electrical
    allSuccess &= mqtt.publish((base + "voltage").c_str(),
                               String(data.voltage.value, 1).c_str());
    allSuccess &= mqtt.publish((base + "current").c_str(),
                               String(data.current.value, 2).c_str());
    allSuccess &= mqtt.publish((base + "power").c_str(),
                               String(data.power, 0).c_str());

    // Pressure
    allSuccess &= mqtt.publish((base + "pressure/high").c_str(),
                               String(data.pressureHigh.value, 0).c_str());
    allSuccess &= mqtt.publish((base + "pressure/low").c_str(),
                               String(data.pressureLow.value, 0).c_str());

    return allSuccess;
}

// ============================================================================
// MQTT CALLBACK FOR INCOMING MESSAGES
// Handle remote commands via MQTT (future feature)
// ============================================================================
void mqttCallback(char* topic, byte* payload, unsigned int length) {
    Serial.print(F("[MQTT] Message received on topic: "));
    Serial.println(topic);

    // Convert payload to string
    String message = "";
    for (unsigned int i = 0; i < length; i++) {
        message += (char)payload[i];
    }

    Serial.print(F("[MQTT] Payload: "));
    Serial.println(message);

    // Handle commands (future implementation)
    // For example: {"command": "status"} or {"command": "reset"}
}

// ============================================================================
// MQTT KEEP-ALIVE / LOOP
// Call this regularly to maintain connection
// ============================================================================
void mqttLoop() {
    if (mqtt.connected()) {
        mqtt.loop();
    }
}

#endif // MQTT_H
