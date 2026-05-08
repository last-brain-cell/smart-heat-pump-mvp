/**
 * @file mqtt.h
 * @brief V2 MQTT publishing interface
 *
 * Publishes 3-phase sensor data as JSON to MQTT broker.
 */

#ifndef MQTT_H
#define MQTT_H

#include <Arduino.h>
#include <PubSubClient.h>
#include "../config.h"
#include "types.h"
#include "globals.h"

bool connectMQTT();
void disconnectMQTT();
bool isMQTTConnected();
bool publishStatus(bool online);
size_t buildJsonPayload(const SystemData& data, char* buffer, size_t bufferSize);
bool publishSensorData(const SystemData& data);
bool publishBufferedData();
void mqttCallback(char* topic, byte* payload, unsigned int length);
void mqttLoop();

#endif // MQTT_H
