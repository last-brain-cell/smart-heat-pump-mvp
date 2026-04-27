#!/bin/sh
set -e

if [ -n "$MQTT_USER" ] && [ -n "$MQTT_PASSWORD" ]; then
  rm -f /mosquitto/config/passwd
  mosquitto_passwd -b -c /mosquitto/config/passwd "$MQTT_USER" "$MQTT_PASSWORD"
fi

exec "$@"
