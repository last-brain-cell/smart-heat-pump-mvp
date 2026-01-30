/**
 * @file mqtt.cpp
 * @brief MQTT publishing implementation
 */

#include "mqtt.h"
#include "gsm.h"
#include "buffer.h"
#include <ArduinoJson.h>

// =============================================================================
// PRIVATE HELPERS
// =============================================================================

/**
 * @brief Build MQTT topic string
 * @param suffix Topic suffix (e.g., "/data", "/status/online")
 * @param buffer Output buffer
 * @param bufferSize Buffer size
 * @return Number of characters written
 */
static size_t buildTopic(const char* suffix, char* buffer, size_t bufferSize) {
    return snprintf(buffer, bufferSize, "%s%s", MQTT_TOPIC_BASE, suffix);
}

// =============================================================================
// IMPLEMENTATION
// =============================================================================

bool connectMQTT() {
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

    mqtt.setServer(MQTT_BROKER, MQTT_PORT);
    mqtt.setCallback(mqttCallback);

    // Build last will topic
    char willTopic[64];
    buildTopic("/status/online", willTopic, sizeof(willTopic));

    bool connected = mqtt.connect(
        MQTT_CLIENT_ID,
        MQTT_USER,
        MQTT_PASS,
        willTopic,
        1,      // QoS
        true,   // Retain
        "false" // Will message
    );

    if (connected) {
        Serial.println(F("[MQTT] Connected!"));

        // Publish online status
        publishStatus(true);

        // Subscribe to command topic
        char cmdTopic[64];
        buildTopic("/commands", cmdTopic, sizeof(cmdTopic));
        mqtt.subscribe(cmdTopic);

        return true;
    }

    Serial.print(F("[MQTT] Connection failed, rc="));
    Serial.println(mqtt.state());
    return false;
}

void disconnectMQTT() {
    if (mqtt.connected()) {
        publishStatus(false);
        mqtt.disconnect();
        Serial.println(F("[MQTT] Disconnected"));
    }
}

bool isMQTTConnected() {
    return mqtt.connected();
}

bool publishStatus(bool online) {
    char topic[64];
    buildTopic("/status/online", topic, sizeof(topic));
    return mqtt.publish(topic, online ? "true" : "false", true);  // Retained
}

size_t buildJsonPayload(const SystemData& data, char* buffer, size_t bufferSize) {
    StaticJsonDocument<JSON_BUFFER_SIZE> doc;

    doc["device"] = DEVICE_ID;
    doc["timestamp"] = data.readingTime;
    doc["version"] = FIRMWARE_VERSION;

    // Temperature object
    JsonObject temps = doc.createNestedObject("temperature");
    temps["inlet"] = round(data.tempInlet.value * 10.0f) / 10.0f;
    temps["outlet"] = round(data.tempOutlet.value * 10.0f) / 10.0f;
    temps["ambient"] = round(data.tempAmbient.value * 10.0f) / 10.0f;
    temps["compressor"] = round(data.tempCompressor.value * 10.0f) / 10.0f;

    // Electrical object
    JsonObject elec = doc.createNestedObject("electrical");
    elec["voltage"] = round(data.voltage.value * 10.0f) / 10.0f;
    elec["current"] = round(data.current.value * 100.0f) / 100.0f;
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

    // Alerts object
    JsonObject alerts = doc.createNestedObject("alerts");
    alerts["voltage"] = static_cast<int>(data.voltage.alertLevel);
    alerts["compressor_temp"] = static_cast<int>(data.tempCompressor.alertLevel);
    alerts["pressure_high"] = static_cast<int>(data.pressureHigh.alertLevel);
    alerts["pressure_low"] = static_cast<int>(data.pressureLow.alertLevel);
    alerts["current"] = static_cast<int>(data.current.alertLevel);

    // Validity flags
    JsonObject valid = doc.createNestedObject("valid");
    valid["temp_inlet"] = data.tempInlet.valid;
    valid["temp_outlet"] = data.tempOutlet.valid;
    valid["temp_ambient"] = data.tempAmbient.valid;
    valid["temp_compressor"] = data.tempCompressor.valid;
    valid["voltage"] = data.voltage.valid;
    valid["current"] = data.current.valid;
    valid["pressure_high"] = data.pressureHigh.valid;
    valid["pressure_low"] = data.pressureLow.valid;

    return serializeJson(doc, buffer, bufferSize);
}

bool publishSensorData(const SystemData& data) {
    if (!mqtt.connected()) {
        Serial.println(F("[MQTT] Not connected, cannot publish"));
        return false;
    }

    char topic[64];
    char payload[JSON_BUFFER_SIZE];

    buildTopic("/data", topic, sizeof(topic));
    buildJsonPayload(data, payload, sizeof(payload));

    Serial.print(F("[MQTT] Publishing to "));
    Serial.println(topic);

    bool success = mqtt.publish(topic, payload);

    if (success) {
        Serial.println(F("[MQTT] Published successfully"));
    } else {
        Serial.println(F("[MQTT] Publish failed!"));
    }

    return success;
}

bool publishBufferedData() {
    if (!mqtt.connected()) {
        Serial.println(F("[MQTT] Not connected, cannot publish buffer"));
        return false;
    }

    uint16_t count = bufferCount();
    if (count == 0) {
        Serial.println(F("[MQTT] Buffer empty, nothing to publish"));
        return true;
    }

    Serial.print(F("[MQTT] Publishing "));
    Serial.print(count);
    Serial.println(F(" buffered readings..."));

    uint16_t published = 0;
    uint16_t failed = 0;

    while (bufferHasData()) {
        SystemData* data = getNextBufferedData();
        if (data == nullptr) {
            break;
        }

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

void mqttCallback(char* topic, byte* payload, unsigned int length) {
    Serial.print(F("[MQTT] Message received on topic: "));
    Serial.println(topic);

    // Convert payload to null-terminated string
    char message[128];
    size_t copyLen = (length < sizeof(message) - 1) ? length : sizeof(message) - 1;
    memcpy(message, payload, copyLen);
    message[copyLen] = '\0';

    Serial.print(F("[MQTT] Payload: "));
    Serial.println(message);

    // Parse JSON command if present
    StaticJsonDocument<128> doc;
    DeserializationError error = deserializeJson(doc, message);

    if (!error && doc.containsKey("command")) {
        const char* command = doc["command"];
        Serial.print(F("[MQTT] Command: "));
        Serial.println(command);
        // Future: handle MQTT commands here
    }
}

void mqttLoop() {
    if (mqtt.connected()) {
        mqtt.loop();
    }
}
