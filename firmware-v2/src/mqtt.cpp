/**
 * @file mqtt.cpp
 * @brief V2 MQTT publishing implementation
 *
 * Key difference from V1: electrical payload contains phase1/phase2/phase3
 * objects with voltage, current, power, energy, frequency, power_factor.
 */

#include "mqtt.h"
#include "gsm.h"
#include "buffer.h"
#include <ArduinoJson.h>

// =============================================================================
// PRIVATE HELPERS
// =============================================================================

static size_t buildTopic(const char* suffix, char* buffer, size_t bufferSize) {
    return snprintf(buffer, bufferSize, "%s%s", mqttTopicBase, suffix);
}

// =============================================================================
// CONNECTION MANAGEMENT (identical to v1)
// =============================================================================

bool connectMQTT() {
    if (activeConnection == CONN_NONE) {
        Log.println(F("[MQTT] No network connection"));
        return false;
    }

    if (mqtt.connected()) {
        Log.println(F("[MQTT] Already connected"));
        return true;
    }

    Log.print(F("[MQTT] Connecting to "));
    Log.print(runtimeCfg.mqttHost);
    Log.print(F(":"));
    Log.println(runtimeCfg.mqttPort);

    mqtt.setServer(runtimeCfg.mqttHost, runtimeCfg.mqttPort);
    mqtt.setCallback(mqttCallback);

    char willTopic[64];
    buildTopic("/status/online", willTopic, sizeof(willTopic));

    bool connected = mqtt.connect(
        deviceId,
        runtimeCfg.mqttUser,
        runtimeCfg.mqttPass,
        willTopic,
        1,      // QoS
        true,   // Retain
        "false" // Will message
    );

    if (connected) {
        Log.println(F("[MQTT] Connected!"));
        publishStatus(true);

        char cmdTopic[64];
        buildTopic("/commands", cmdTopic, sizeof(cmdTopic));
        mqtt.subscribe(cmdTopic);

        return true;
    }

    Log.print(F("[MQTT] Connection failed, rc="));
    Log.println(mqtt.state());
    return false;
}

void disconnectMQTT() {
    if (mqtt.connected()) {
        publishStatus(false);
        mqtt.disconnect();
        Log.println(F("[MQTT] Disconnected"));
    }
}

bool isMQTTConnected() {
    return mqtt.connected();
}

bool publishStatus(bool online) {
    char topic[64];
    buildTopic("/status/online", topic, sizeof(topic));
    return mqtt.publish(topic, online ? "true" : "false", true);
}

// =============================================================================
// V2 JSON PAYLOAD BUILDER (3-phase)
// =============================================================================

size_t buildJsonPayload(const SystemData& data, char* buffer, size_t bufferSize) {
    StaticJsonDocument<JSON_BUFFER_SIZE> doc;

    doc["device"] = deviceId;
    doc["timestamp"] = data.readingTime;
    doc["version"] = FIRMWARE_VERSION;

    // Temperature (only inlet + outlet in v2)
    JsonObject temps = doc.createNestedObject("temperature");
    temps["inlet"]  = round(data.tempInlet.value * 10.0f) / 10.0f;
    temps["outlet"] = round(data.tempOutlet.value * 10.0f) / 10.0f;

    // 3-phase electrical
    JsonObject elec = doc.createNestedObject("electrical");
    const char* phaseNames[] = {"phase1", "phase2", "phase3"};

    for (int i = 0; i < 3; i++) {
        JsonObject ph = elec.createNestedObject(phaseNames[i]);
        ph["voltage"]      = round(data.phase[i].voltage.value * 10.0f) / 10.0f;
        ph["current"]      = round(data.phase[i].current.value * 100.0f) / 100.0f;
        ph["power"]        = round(data.phase[i].power * 10.0f) / 10.0f;
        ph["energy"]       = round(data.phase[i].energy * 100.0f) / 100.0f;
        ph["frequency"]    = round(data.phase[i].frequency * 10.0f) / 10.0f;
        ph["power_factor"] = round(data.phase[i].powerFactor * 100.0f) / 100.0f;
    }

    // Status
    JsonObject status = doc.createNestedObject("status");
    status["compressor"] = data.compressorRunning;
    status["fan"]        = data.fanRunning;
    status["defrost"]    = data.defrostActive;

    // Per-phase alerts
    JsonObject alerts = doc.createNestedObject("alerts");
    alerts["voltage_p1"] = static_cast<int>(data.phase[0].voltage.alertLevel);
    alerts["voltage_p2"] = static_cast<int>(data.phase[1].voltage.alertLevel);
    alerts["voltage_p3"] = static_cast<int>(data.phase[2].voltage.alertLevel);
    alerts["current_p1"] = static_cast<int>(data.phase[0].current.alertLevel);
    alerts["current_p2"] = static_cast<int>(data.phase[1].current.alertLevel);
    alerts["current_p3"] = static_cast<int>(data.phase[2].current.alertLevel);

    // Validity flags
    JsonObject valid = doc.createNestedObject("valid");
    valid["temp_inlet"]  = data.tempInlet.valid;
    valid["temp_outlet"] = data.tempOutlet.valid;
    valid["phase1"]      = data.phase[0].valid;
    valid["phase2"]      = data.phase[1].valid;
    valid["phase3"]      = data.phase[2].valid;

    return serializeJson(doc, buffer, bufferSize);
}

// =============================================================================
// PUBLISH (identical structure to v1, uses v2 buildJsonPayload above)
// =============================================================================

bool publishSensorData(const SystemData& data) {
    if (!mqtt.connected()) {
        Log.println(F("[MQTT] Not connected, cannot publish"));
        return false;
    }

    char topic[64];
    char payload[JSON_BUFFER_SIZE];

    buildTopic("/data", topic, sizeof(topic));
    buildJsonPayload(data, payload, sizeof(payload));

    Log.print(F("[MQTT] Publishing to "));
    Log.println(topic);

    bool success = mqtt.publish(topic, payload);

    if (success) {
        Log.println(F("[MQTT] Published successfully"));
    } else {
        Log.println(F("[MQTT] Publish failed!"));
    }

    return success;
}

bool publishBufferedData() {
    if (!mqtt.connected()) {
        Log.println(F("[MQTT] Not connected, cannot publish buffer"));
        return false;
    }

    uint16_t count = bufferCount();
    if (count == 0) {
        Log.println(F("[MQTT] Buffer empty, nothing to publish"));
        return true;
    }

    Log.print(F("[MQTT] Publishing "));
    Log.print(count);
    Log.println(F(" buffered readings..."));

    uint16_t published = 0;
    uint16_t failed = 0;

    while (bufferHasData()) {
        SystemData* data = getNextBufferedData();
        if (data == nullptr) break;

        if (publishSensorData(*data)) {
            markDataPublished();
            published++;
        } else {
            failed++;
            break;  // Stop on first failure to preserve order
        }

        mqtt.loop();
        delay(100);
    }

    Log.print(F("[MQTT] Published: "));
    Log.print(published);
    Log.print(F(", Failed: "));
    Log.println(failed);

    return (failed == 0);
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
    Log.print(F("[MQTT] Message received on topic: "));
    Log.println(topic);

    char message[128];
    size_t copyLen = (length < sizeof(message) - 1) ? length : sizeof(message) - 1;
    memcpy(message, payload, copyLen);
    message[copyLen] = '\0';

    Log.print(F("[MQTT] Payload: "));
    Log.println(message);

    StaticJsonDocument<128> doc;
    DeserializationError error = deserializeJson(doc, message);

    if (!error && doc.containsKey("command")) {
        const char* command = doc["command"];
        Log.print(F("[MQTT] Command: "));
        Log.println(command);
    }
}

void mqttLoop() {
    if (mqtt.connected()) {
        mqtt.loop();
    }
}
