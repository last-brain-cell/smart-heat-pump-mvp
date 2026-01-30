/**
 * @file mqtt.h
 * @brief MQTT publishing interface
 *
 * Handles MQTT connection and publishing sensor data over GPRS.
 * Uses PubSubClient library with TinyGSM client.
 */

#ifndef MQTT_H
#define MQTT_H

#include <Arduino.h>
#include <PubSubClient.h>
#include "../config.h"
#include "types.h"
#include "globals.h"

// =============================================================================
// FUNCTION DECLARATIONS
// =============================================================================

/**
 * @brief Connect to MQTT broker
 * @return true if connection successful
 */
bool connectMQTT();

/**
 * @brief Disconnect from MQTT broker
 */
void disconnectMQTT();

/**
 * @brief Check if connected to MQTT broker
 * @return true if connected
 */
bool isMQTTConnected();

/**
 * @brief Publish sensor data to MQTT
 * @param data Sensor data to publish
 * @return true if published successfully
 */
bool publishSensorData(const SystemData& data);

/**
 * @brief Publish all buffered data to MQTT
 * @return true if all data published successfully
 */
bool publishBufferedData();

/**
 * @brief Publish online/offline status
 * @param online true for online, false for offline
 * @return true if published successfully
 */
bool publishStatus(bool online);

/**
 * @brief MQTT message callback handler
 * @param topic Topic the message was received on
 * @param payload Message payload
 * @param length Payload length
 */
void mqttCallback(char* topic, byte* payload, unsigned int length);

/**
 * @brief Build JSON payload from sensor data
 * @param data Sensor data to serialize
 * @param buffer Output buffer
 * @param bufferSize Size of output buffer
 * @return Number of characters written
 */
size_t buildJsonPayload(const SystemData& data, char* buffer, size_t bufferSize);

/**
 * @brief Process MQTT client loop
 * @note Call regularly to maintain connection
 */
void mqttLoop();

#endif // MQTT_H
