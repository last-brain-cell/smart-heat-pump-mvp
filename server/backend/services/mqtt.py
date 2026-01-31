"""
MQTT Service
============

MQTT client service for receiving data from ESP32 devices.

Subscribes to topics:
    - heatpump/+/data: Sensor readings
    - heatpump/+/status/#: Device status updates
    - heatpump/+/alerts: Alert notifications
"""

import json
import asyncio
from typing import Optional, Dict, Callable
from datetime import datetime, timezone

import paho.mqtt.client as mqtt

from core.config import settings
from core.logging import get_logger

logger = get_logger(__name__)


class MQTTService:
    """
    MQTT client service for receiving data from ESP32 devices.

    When data is received:
        1. Parse the JSON payload
        2. Call registered handlers (for storage, broadcasting, etc.)
    """

    def __init__(self):
        """Initialize MQTT client."""
        self.client: Optional[mqtt.Client] = None
        self._connected = False
        self._on_sensor_data: Optional[Callable] = None
        self._on_alert: Optional[Callable] = None

    def set_handlers(
        self,
        on_sensor_data: Optional[Callable] = None,
        on_alert: Optional[Callable] = None,
    ) -> None:
        """
        Set callback handlers for MQTT messages.

        Args:
            on_sensor_data: Callback for sensor data messages
            on_alert: Callback for alert messages
        """
        self._on_sensor_data = on_sensor_data
        self._on_alert = on_alert

    def _on_connect(self, client, userdata, flags, rc) -> None:
        """Callback when MQTT connection is established."""
        if rc == 0:
            logger.info(f"MQTT connected to {settings.MQTT_BROKER}:{settings.MQTT_PORT}")
            self._connected = True

            # Subscribe to topics
            client.subscribe(settings.MQTT_TOPIC_DATA)
            client.subscribe(settings.MQTT_TOPIC_STATUS)
            client.subscribe(settings.MQTT_TOPIC_ALERTS)

            logger.info("MQTT subscribed to topics")
        else:
            logger.error(f"MQTT connection failed with code {rc}")
            self._connected = False

    def _on_disconnect(self, client, userdata, rc) -> None:
        """Callback when MQTT connection is lost."""
        logger.warning(f"MQTT disconnected (rc={rc})")
        self._connected = False

    def _on_message(self, client, userdata, msg) -> None:
        """Callback when MQTT message is received."""
        try:
            topic = msg.topic
            payload = json.loads(msg.payload.decode())

            logger.debug(f"MQTT message on {topic}")

            # Extract device_id from topic: heatpump/{device_id}/...
            parts = topic.split("/")
            if len(parts) >= 2:
                device_id = parts[1]
            else:
                logger.warning(f"Invalid topic format: {topic}")
                return

            # Handle data messages
            if "/data" in topic:
                self._handle_sensor_data(device_id, payload)

            # Handle alert messages
            elif "/alerts" in topic:
                self._handle_alert(device_id, payload)

        except json.JSONDecodeError as e:
            logger.error(f"Failed to parse MQTT payload: {e}")
        except Exception as e:
            logger.error(f"Error processing MQTT message: {e}")

    def _handle_sensor_data(self, device_id: str, data: Dict) -> None:
        """Process incoming sensor data."""
        if self._on_sensor_data:
            self._on_sensor_data(device_id, data)

    def _handle_alert(self, device_id: str, data: Dict) -> None:
        """Process incoming alert notification."""
        if self._on_alert:
            alert_type = data.get("type", "unknown")
            level = data.get("level", "warning")
            value = data.get("value", 0)
            message = data.get("message", "")
            self._on_alert(device_id, alert_type, level, value, message)

    def connect(self) -> bool:
        """
        Connect to MQTT broker.

        Returns:
            bool: True if connection initiated successfully
        """
        try:
            self.client = mqtt.Client()
            self.client.username_pw_set(settings.MQTT_USER, settings.MQTT_PASSWORD)
            self.client.on_connect = self._on_connect
            self.client.on_disconnect = self._on_disconnect
            self.client.on_message = self._on_message

            self.client.connect(settings.MQTT_BROKER, settings.MQTT_PORT, 60)
            self.client.loop_start()

            logger.info(f"MQTT client started, connecting to {settings.MQTT_BROKER}")
            return True

        except Exception as e:
            logger.error(f"MQTT connection failed: {e}")
            return False

    def disconnect(self) -> None:
        """Disconnect from MQTT broker."""
        if self.client:
            self.client.loop_stop()
            self.client.disconnect()
            logger.info("MQTT disconnected")

    def is_connected(self) -> bool:
        """Check if MQTT is connected."""
        return self._connected

    def publish(self, topic: str, payload: Dict) -> bool:
        """
        Publish a message to MQTT broker.

        Args:
            topic: MQTT topic
            payload: Message payload (will be JSON encoded)

        Returns:
            bool: True if publish successful
        """
        if not self._connected or not self.client:
            logger.warning("Cannot publish - MQTT not connected")
            return False

        try:
            result = self.client.publish(topic, json.dumps(payload))
            return result.rc == mqtt.MQTT_ERR_SUCCESS
        except Exception as e:
            logger.error(f"Failed to publish MQTT message: {e}")
            return False


# Global service instance
mqtt_service = MQTTService()
