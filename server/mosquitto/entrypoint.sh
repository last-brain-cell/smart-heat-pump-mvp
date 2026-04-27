#!/bin/sh
set -e

if [ -n "$MQTT_USER" ] && [ -n "$MQTT_PASSWORD" ]; then
  : > /mosquitto/config/passwd
  mosquitto_passwd -b /mosquitto/config/passwd "$MQTT_USER" "$MQTT_PASSWORD"
fi

exec "$@"
