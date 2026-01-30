"""
Heat Pump Monitoring System - Backend API
==========================================

This module provides the REST API backend for the heat pump monitoring system.
It handles:
    - MQTT subscription for receiving sensor data from ESP32 devices
    - Data storage in InfluxDB (time-series database)
    - REST API endpoints for the web dashboard
    - WebSocket connections for real-time updates

Architecture Overview:
    ESP32 Device --> MQTT Broker --> This Backend --> InfluxDB
                                          |
                                          v
                                    Web Dashboard

Author: Heat Pump Project
Version: 1.0.0
License: MIT
"""

import os
import json
import asyncio
import logging
from datetime import datetime, timedelta, timezone
from contextlib import asynccontextmanager
from typing import Optional, List, Dict, Any

from fastapi import FastAPI, HTTPException, WebSocket, WebSocketDisconnect, Query
from fastapi.middleware.cors import CORSMiddleware
from pydantic import BaseModel, Field
import paho.mqtt.client as mqtt

from influxdb_client import InfluxDBClient, Point, WritePrecision
from influxdb_client.client.write_api import SYNCHRONOUS

# =============================================================================
# LOGGING CONFIGURATION
# =============================================================================

logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s - %(name)s - %(levelname)s - %(message)s'
)
logger = logging.getLogger(__name__)

# =============================================================================
# CONFIGURATION
# =============================================================================

class Config:
    """
    Application configuration loaded from environment variables.

    Environment Variables:
        INFLUXDB_URL: InfluxDB server URL (default: http://localhost:8086)
        INFLUXDB_TOKEN: Authentication token for InfluxDB
        INFLUXDB_ORG: InfluxDB organization name
        INFLUXDB_BUCKET: InfluxDB bucket for sensor data
        MQTT_BROKER: MQTT broker hostname (default: localhost)
        MQTT_PORT: MQTT broker port (default: 1883)
        MQTT_USER: MQTT authentication username
        MQTT_PASSWORD: MQTT authentication password
    """
    # InfluxDB Configuration
    INFLUXDB_URL: str = os.getenv("INFLUXDB_URL", "http://localhost:8086")
    INFLUXDB_TOKEN: str = os.getenv("INFLUXDB_TOKEN", "heatpump-super-secret-token")
    INFLUXDB_ORG: str = os.getenv("INFLUXDB_ORG", "heatpump")
    INFLUXDB_BUCKET: str = os.getenv("INFLUXDB_BUCKET", "sensor_data")

    # MQTT Configuration
    MQTT_BROKER: str = os.getenv("MQTT_BROKER", "localhost")
    MQTT_PORT: int = int(os.getenv("MQTT_PORT", "1883"))
    MQTT_USER: str = os.getenv("MQTT_USER", "heatpump")
    MQTT_PASSWORD: str = os.getenv("MQTT_PASSWORD", "heatpump123")

    # MQTT Topics
    MQTT_TOPIC_DATA: str = "heatpump/+/data"
    MQTT_TOPIC_STATUS: str = "heatpump/+/status/#"
    MQTT_TOPIC_ALERTS: str = "heatpump/+/alerts"


config = Config()

# =============================================================================
# PYDANTIC MODELS (Request/Response Schemas)
# =============================================================================

class TemperatureReading(BaseModel):
    """Temperature sensor readings in Celsius."""
    inlet: Optional[float] = Field(None, description="Water/refrigerant inlet temperature")
    outlet: Optional[float] = Field(None, description="Water/refrigerant outlet temperature")
    ambient: Optional[float] = Field(None, description="Ambient air temperature")
    compressor: Optional[float] = Field(None, description="Compressor body temperature")


class ElectricalReading(BaseModel):
    """Electrical sensor readings."""
    voltage: Optional[float] = Field(None, description="AC voltage in Volts")
    current: Optional[float] = Field(None, description="Current draw in Amps")
    power: Optional[float] = Field(None, description="Power consumption in Watts")


class PressureReading(BaseModel):
    """Pressure sensor readings in PSI."""
    high: Optional[float] = Field(None, description="High side refrigerant pressure")
    low: Optional[float] = Field(None, description="Low side refrigerant pressure")


class SystemStatus(BaseModel):
    """Heat pump system status flags."""
    compressor: Optional[bool] = Field(None, description="Compressor running state")
    fan: Optional[bool] = Field(None, description="Fan running state")
    defrost: Optional[bool] = Field(None, description="Defrost mode active")


class SensorDataPayload(BaseModel):
    """
    Complete sensor data payload from ESP32 device.

    This matches the JSON structure published by the ESP32 firmware
    to the MQTT topic: heatpump/{device_id}/data
    """
    device: str = Field(..., description="Device identifier")
    timestamp: Optional[int] = Field(None, description="Unix timestamp in milliseconds")
    temperature: Optional[TemperatureReading] = None
    electrical: Optional[ElectricalReading] = None
    pressure: Optional[PressureReading] = None
    status: Optional[SystemStatus] = None
    alerts: Optional[Dict[str, int]] = Field(None, description="Alert levels (0=OK, 1=Warning, 2=Critical)")


class SensorDataResponse(BaseModel):
    """Response model for sensor readings from the database."""
    time: datetime = Field(..., description="Timestamp of the reading")
    device_id: str = Field(..., description="Device identifier")
    temp_inlet: Optional[float] = None
    temp_outlet: Optional[float] = None
    temp_ambient: Optional[float] = None
    temp_compressor: Optional[float] = None
    voltage: Optional[float] = None
    current: Optional[float] = None
    power: Optional[float] = None
    pressure_high: Optional[float] = None
    pressure_low: Optional[float] = None
    compressor_running: Optional[bool] = None

    class Config:
        json_schema_extra = {
            "example": {
                "time": "2024-01-15T10:30:00Z",
                "device_id": "site1",
                "temp_inlet": 45.2,
                "temp_outlet": 50.1,
                "temp_ambient": 25.0,
                "temp_compressor": 70.5,
                "voltage": 230.5,
                "current": 8.5,
                "power": 1959.25,
                "pressure_high": 280.0,
                "pressure_low": 70.0,
                "compressor_running": True
            }
        }


class DeviceStatusResponse(BaseModel):
    """Current status of a heat pump device."""
    device_id: str = Field(..., description="Device identifier")
    is_online: bool = Field(..., description="Whether device has sent data recently")
    last_seen: Optional[datetime] = Field(None, description="Last data received timestamp")
    temp_inlet: Optional[float] = None
    temp_outlet: Optional[float] = None
    temp_ambient: Optional[float] = None
    temp_compressor: Optional[float] = None
    voltage: Optional[float] = None
    current: Optional[float] = None
    power: Optional[float] = None
    pressure_high: Optional[float] = None
    pressure_low: Optional[float] = None
    compressor_running: Optional[bool] = None


class AlertResponse(BaseModel):
    """Alert record from the database."""
    time: datetime = Field(..., description="When the alert occurred")
    device_id: str = Field(..., description="Device that generated the alert")
    alert_type: str = Field(..., description="Type of alert (voltage_high, compressor_temp, etc.)")
    alert_level: str = Field(..., description="Severity level (warning, critical)")
    value: Optional[float] = Field(None, description="Sensor value that triggered the alert")
    message: Optional[str] = Field(None, description="Alert message")


class StatsResponse(BaseModel):
    """System-wide statistics."""
    total_readings: int = Field(..., description="Total sensor readings in database")
    total_alerts: int = Field(..., description="Total alerts recorded")
    devices_online: int = Field(..., description="Number of devices online")
    oldest_reading: Optional[datetime] = Field(None, description="Oldest reading timestamp")
    newest_reading: Optional[datetime] = Field(None, description="Most recent reading timestamp")


class HealthResponse(BaseModel):
    """API health check response."""
    status: str = Field(..., description="Health status (healthy/unhealthy)")
    influxdb_connected: bool = Field(..., description="InfluxDB connection status")
    mqtt_connected: bool = Field(..., description="MQTT broker connection status")
    timestamp: datetime = Field(..., description="Health check timestamp")


# =============================================================================
# INFLUXDB CLIENT
# =============================================================================

class InfluxDBService:
    """
    Service class for InfluxDB operations.

    Provides methods for:
        - Writing sensor data points
        - Querying historical data
        - Getting device status
        - Retrieving statistics

    Uses the InfluxDB 2.x Python client with Flux query language.
    """

    def __init__(self):
        """Initialize InfluxDB client connection."""
        self.client: Optional[InfluxDBClient] = None
        self.write_api = None
        self.query_api = None

    def connect(self) -> bool:
        """
        Establish connection to InfluxDB.

        Returns:
            bool: True if connection successful, False otherwise
        """
        try:
            self.client = InfluxDBClient(
                url=config.INFLUXDB_URL,
                token=config.INFLUXDB_TOKEN,
                org=config.INFLUXDB_ORG
            )
            self.write_api = self.client.write_api(write_options=SYNCHRONOUS)
            self.query_api = self.client.query_api()

            # Test connection
            health = self.client.health()
            logger.info(f"InfluxDB connected: {health.status}")
            return health.status == "pass"
        except Exception as e:
            logger.error(f"InfluxDB connection failed: {e}")
            return False

    def disconnect(self):
        """Close InfluxDB connection."""
        if self.client:
            self.client.close()
            logger.info("InfluxDB disconnected")

    def is_connected(self) -> bool:
        """Check if InfluxDB is connected and healthy."""
        try:
            if self.client:
                health = self.client.health()
                return health.status == "pass"
        except:
            pass
        return False

    def write_sensor_data(self, device_id: str, data: Dict[str, Any]) -> bool:
        """
        Write sensor data point to InfluxDB.

        Args:
            device_id: Unique device identifier
            data: Sensor data dictionary from MQTT payload

        Returns:
            bool: True if write successful

        Data is written as a single point with multiple fields:
            - Measurement: "sensor_reading"
            - Tag: device_id
            - Fields: All sensor values
        """
        try:
            point = Point("sensor_reading").tag("device_id", device_id)

            # Add temperature fields
            temps = data.get("temperature", {})
            if temps.get("inlet") is not None:
                point.field("temp_inlet", float(temps["inlet"]))
            if temps.get("outlet") is not None:
                point.field("temp_outlet", float(temps["outlet"]))
            if temps.get("ambient") is not None:
                point.field("temp_ambient", float(temps["ambient"]))
            if temps.get("compressor") is not None:
                point.field("temp_compressor", float(temps["compressor"]))

            # Add electrical fields
            elec = data.get("electrical", {})
            if elec.get("voltage") is not None:
                point.field("voltage", float(elec["voltage"]))
            if elec.get("current") is not None:
                point.field("current", float(elec["current"]))
            if elec.get("power") is not None:
                point.field("power", float(elec["power"]))

            # Add pressure fields
            pressure = data.get("pressure", {})
            if pressure.get("high") is not None:
                point.field("pressure_high", float(pressure["high"]))
            if pressure.get("low") is not None:
                point.field("pressure_low", float(pressure["low"]))

            # Add status fields
            status = data.get("status", {})
            if status.get("compressor") is not None:
                point.field("compressor_running", bool(status["compressor"]))
            if status.get("fan") is not None:
                point.field("fan_running", bool(status["fan"]))
            if status.get("defrost") is not None:
                point.field("defrost_active", bool(status["defrost"]))

            # Write to InfluxDB
            self.write_api.write(
                bucket=config.INFLUXDB_BUCKET,
                org=config.INFLUXDB_ORG,
                record=point,
                write_precision=WritePrecision.MS
            )

            logger.debug(f"Wrote sensor data for device {device_id}")
            return True

        except Exception as e:
            logger.error(f"Failed to write sensor data: {e}")
            return False

    def write_alert(self, device_id: str, alert_type: str, level: str,
                    value: float, message: str = None) -> bool:
        """
        Write an alert record to InfluxDB.

        Args:
            device_id: Device that generated the alert
            alert_type: Type of alert (e.g., "voltage_high", "compressor_temp")
            level: Severity level ("warning" or "critical")
            value: Sensor value that triggered the alert
            message: Optional alert message

        Returns:
            bool: True if write successful
        """
        try:
            point = (
                Point("alert")
                .tag("device_id", device_id)
                .tag("alert_type", alert_type)
                .tag("level", level)
                .field("value", float(value))
            )

            if message:
                point.field("message", message)

            self.write_api.write(
                bucket=config.INFLUXDB_BUCKET,
                org=config.INFLUXDB_ORG,
                record=point
            )

            logger.info(f"Alert recorded: {device_id} - {alert_type} ({level})")
            return True

        except Exception as e:
            logger.error(f"Failed to write alert: {e}")
            return False

    def get_latest_reading(self, device_id: str) -> Optional[Dict]:
        """
        Get the most recent sensor reading for a device.

        Args:
            device_id: Device identifier

        Returns:
            Dict with latest sensor values or None if no data
        """
        query = f'''
        from(bucket: "{config.INFLUXDB_BUCKET}")
            |> range(start: -24h)
            |> filter(fn: (r) => r["_measurement"] == "sensor_reading")
            |> filter(fn: (r) => r["device_id"] == "{device_id}")
            |> last()
            |> pivot(rowKey: ["_time"], columnKey: ["_field"], valueColumn: "_value")
        '''

        try:
            tables = self.query_api.query(query, org=config.INFLUXDB_ORG)

            for table in tables:
                for record in table.records:
                    return {
                        "time": record.get_time(),
                        "device_id": device_id,
                        "temp_inlet": record.values.get("temp_inlet"),
                        "temp_outlet": record.values.get("temp_outlet"),
                        "temp_ambient": record.values.get("temp_ambient"),
                        "temp_compressor": record.values.get("temp_compressor"),
                        "voltage": record.values.get("voltage"),
                        "current": record.values.get("current"),
                        "power": record.values.get("power"),
                        "pressure_high": record.values.get("pressure_high"),
                        "pressure_low": record.values.get("pressure_low"),
                        "compressor_running": record.values.get("compressor_running"),
                    }
            return None

        except Exception as e:
            logger.error(f"Failed to get latest reading: {e}")
            return None

    def get_readings(self, device_id: str, hours: int = 24,
                     limit: int = 100) -> List[Dict]:
        """
        Get historical sensor readings for a device.

        Args:
            device_id: Device identifier
            hours: How many hours of history to retrieve
            limit: Maximum number of records to return

        Returns:
            List of sensor reading dictionaries, newest first
        """
        query = f'''
        from(bucket: "{config.INFLUXDB_BUCKET}")
            |> range(start: -{hours}h)
            |> filter(fn: (r) => r["_measurement"] == "sensor_reading")
            |> filter(fn: (r) => r["device_id"] == "{device_id}")
            |> pivot(rowKey: ["_time"], columnKey: ["_field"], valueColumn: "_value")
            |> sort(columns: ["_time"], desc: true)
            |> limit(n: {limit})
        '''

        try:
            tables = self.query_api.query(query, org=config.INFLUXDB_ORG)
            readings = []

            for table in tables:
                for record in table.records:
                    readings.append({
                        "time": record.get_time(),
                        "device_id": device_id,
                        "temp_inlet": record.values.get("temp_inlet"),
                        "temp_outlet": record.values.get("temp_outlet"),
                        "temp_ambient": record.values.get("temp_ambient"),
                        "temp_compressor": record.values.get("temp_compressor"),
                        "voltage": record.values.get("voltage"),
                        "current": record.values.get("current"),
                        "power": record.values.get("power"),
                        "pressure_high": record.values.get("pressure_high"),
                        "pressure_low": record.values.get("pressure_low"),
                        "compressor_running": record.values.get("compressor_running"),
                    })

            return readings

        except Exception as e:
            logger.error(f"Failed to get readings: {e}")
            return []

    def get_device_list(self) -> List[str]:
        """
        Get list of all device IDs that have sent data.

        Returns:
            List of unique device identifiers
        """
        query = f'''
        from(bucket: "{config.INFLUXDB_BUCKET}")
            |> range(start: -30d)
            |> filter(fn: (r) => r["_measurement"] == "sensor_reading")
            |> keep(columns: ["device_id"])
            |> distinct(column: "device_id")
        '''

        try:
            tables = self.query_api.query(query, org=config.INFLUXDB_ORG)
            devices = []

            for table in tables:
                for record in table.records:
                    device_id = record.values.get("device_id")
                    if device_id and device_id not in devices:
                        devices.append(device_id)

            return devices

        except Exception as e:
            logger.error(f"Failed to get device list: {e}")
            return []

    def get_stats(self) -> Dict:
        """
        Get system-wide statistics.

        Returns:
            Dictionary with total readings, alerts, device count, etc.
        """
        stats = {
            "total_readings": 0,
            "total_alerts": 0,
            "devices_online": 0,
            "oldest_reading": None,
            "newest_reading": None,
        }

        try:
            # Count total readings
            query = f'''
            from(bucket: "{config.INFLUXDB_BUCKET}")
                |> range(start: -30d)
                |> filter(fn: (r) => r["_measurement"] == "sensor_reading")
                |> count()
                |> sum()
            '''
            tables = self.query_api.query(query, org=config.INFLUXDB_ORG)
            for table in tables:
                for record in table.records:
                    stats["total_readings"] = int(record.get_value() or 0)

            # Count alerts
            query = f'''
            from(bucket: "{config.INFLUXDB_BUCKET}")
                |> range(start: -30d)
                |> filter(fn: (r) => r["_measurement"] == "alert")
                |> count()
            '''
            tables = self.query_api.query(query, org=config.INFLUXDB_ORG)
            for table in tables:
                for record in table.records:
                    stats["total_alerts"] += int(record.get_value() or 0)

            # Count online devices (active in last 5 minutes)
            query = f'''
            from(bucket: "{config.INFLUXDB_BUCKET}")
                |> range(start: -5m)
                |> filter(fn: (r) => r["_measurement"] == "sensor_reading")
                |> keep(columns: ["device_id"])
                |> distinct(column: "device_id")
                |> count()
            '''
            tables = self.query_api.query(query, org=config.INFLUXDB_ORG)
            for table in tables:
                for record in table.records:
                    stats["devices_online"] = int(record.get_value() or 0)

        except Exception as e:
            logger.error(f"Failed to get stats: {e}")

        return stats

    def get_alerts(self, device_id: str = None, hours: int = 24,
                   limit: int = 50) -> List[Dict]:
        """
        Get alert history.

        Args:
            device_id: Optional filter by device
            hours: How many hours of history
            limit: Maximum records to return

        Returns:
            List of alert dictionaries
        """
        device_filter = ""
        if device_id:
            device_filter = f'|> filter(fn: (r) => r["device_id"] == "{device_id}")'

        query = f'''
        from(bucket: "{config.INFLUXDB_BUCKET}")
            |> range(start: -{hours}h)
            |> filter(fn: (r) => r["_measurement"] == "alert")
            {device_filter}
            |> pivot(rowKey: ["_time"], columnKey: ["_field"], valueColumn: "_value")
            |> sort(columns: ["_time"], desc: true)
            |> limit(n: {limit})
        '''

        try:
            tables = self.query_api.query(query, org=config.INFLUXDB_ORG)
            alerts = []

            for table in tables:
                for record in table.records:
                    alerts.append({
                        "time": record.get_time(),
                        "device_id": record.values.get("device_id"),
                        "alert_type": record.values.get("alert_type"),
                        "alert_level": record.values.get("level"),
                        "value": record.values.get("value"),
                        "message": record.values.get("message"),
                    })

            return alerts

        except Exception as e:
            logger.error(f"Failed to get alerts: {e}")
            return []


# Global InfluxDB service instance
influxdb_service = InfluxDBService()

# =============================================================================
# WEBSOCKET CONNECTION MANAGER
# =============================================================================

class WebSocketManager:
    """
    Manages WebSocket connections for real-time updates.

    Allows the backend to broadcast sensor data updates to all
    connected dashboard clients immediately when new data arrives
    via MQTT.
    """

    def __init__(self):
        """Initialize the connection manager."""
        self.active_connections: List[WebSocket] = []

    async def connect(self, websocket: WebSocket):
        """
        Accept a new WebSocket connection.

        Args:
            websocket: The WebSocket connection to add
        """
        await websocket.accept()
        self.active_connections.append(websocket)
        logger.info(f"WebSocket connected. Total: {len(self.active_connections)}")

    def disconnect(self, websocket: WebSocket):
        """
        Remove a WebSocket connection.

        Args:
            websocket: The WebSocket connection to remove
        """
        if websocket in self.active_connections:
            self.active_connections.remove(websocket)
        logger.info(f"WebSocket disconnected. Total: {len(self.active_connections)}")

    async def broadcast(self, message: Dict):
        """
        Send a message to all connected clients.

        Args:
            message: Dictionary to send as JSON
        """
        disconnected = []

        for connection in self.active_connections:
            try:
                await connection.send_json(message)
            except Exception as e:
                logger.warning(f"Failed to send to WebSocket: {e}")
                disconnected.append(connection)

        # Clean up disconnected clients
        for conn in disconnected:
            self.disconnect(conn)


# Global WebSocket manager instance
ws_manager = WebSocketManager()

# =============================================================================
# MQTT CLIENT
# =============================================================================

class MQTTService:
    """
    MQTT client service for receiving data from ESP32 devices.

    Subscribes to topics:
        - heatpump/+/data: Sensor readings
        - heatpump/+/status/#: Device status updates
        - heatpump/+/alerts: Alert notifications

    When data is received:
        1. Parse the JSON payload
        2. Store in InfluxDB
        3. Broadcast to WebSocket clients
    """

    def __init__(self):
        """Initialize MQTT client."""
        self.client: Optional[mqtt.Client] = None
        self._connected = False

    def _on_connect(self, client, userdata, flags, rc):
        """
        Callback when MQTT connection is established.

        Args:
            rc: Return code (0 = success)
        """
        if rc == 0:
            logger.info(f"MQTT connected to {config.MQTT_BROKER}:{config.MQTT_PORT}")
            self._connected = True

            # Subscribe to topics
            client.subscribe(config.MQTT_TOPIC_DATA)
            client.subscribe(config.MQTT_TOPIC_STATUS)
            client.subscribe(config.MQTT_TOPIC_ALERTS)

            logger.info("MQTT subscribed to topics")
        else:
            logger.error(f"MQTT connection failed with code {rc}")
            self._connected = False

    def _on_disconnect(self, client, userdata, rc):
        """Callback when MQTT connection is lost."""
        logger.warning(f"MQTT disconnected (rc={rc})")
        self._connected = False

    def _on_message(self, client, userdata, msg):
        """
        Callback when MQTT message is received.

        Processes messages from:
            - heatpump/{device_id}/data: Sensor readings
            - heatpump/{device_id}/alerts: Alert notifications
        """
        try:
            topic = msg.topic
            payload = json.loads(msg.payload.decode())

            logger.debug(f"MQTT message on {topic}")

            # Extract device_id from topic: heatpump/{device_id}/...
            parts = topic.split('/')
            if len(parts) >= 2:
                device_id = parts[1]
            else:
                logger.warning(f"Invalid topic format: {topic}")
                return

            # Handle data messages
            if '/data' in topic:
                self._handle_sensor_data(device_id, payload)

            # Handle alert messages
            elif '/alerts' in topic:
                self._handle_alert(device_id, payload)

        except json.JSONDecodeError as e:
            logger.error(f"Failed to parse MQTT payload: {e}")
        except Exception as e:
            logger.error(f"Error processing MQTT message: {e}")

    def _handle_sensor_data(self, device_id: str, data: Dict):
        """
        Process incoming sensor data.

        Args:
            device_id: Device identifier
            data: Sensor data payload
        """
        # Store in InfluxDB
        influxdb_service.write_sensor_data(device_id, data)

        # Broadcast to WebSocket clients
        asyncio.run(ws_manager.broadcast({
            "type": "sensor_data",
            "device_id": device_id,
            "data": data,
            "timestamp": datetime.now(timezone.utc).isoformat()
        }))

    def _handle_alert(self, device_id: str, data: Dict):
        """
        Process incoming alert notification.

        Args:
            device_id: Device identifier
            data: Alert payload
        """
        alert_type = data.get("type", "unknown")
        level = data.get("level", "warning")
        value = data.get("value", 0)
        message = data.get("message", "")

        # Store in InfluxDB
        influxdb_service.write_alert(device_id, alert_type, level, value, message)

        # Broadcast to WebSocket clients
        asyncio.run(ws_manager.broadcast({
            "type": "alert",
            "device_id": device_id,
            "alert_type": alert_type,
            "level": level,
            "value": value,
            "message": message,
            "timestamp": datetime.now(timezone.utc).isoformat()
        }))

    def connect(self) -> bool:
        """
        Connect to MQTT broker.

        Returns:
            bool: True if connection initiated successfully
        """
        try:
            self.client = mqtt.Client()
            self.client.username_pw_set(config.MQTT_USER, config.MQTT_PASSWORD)
            self.client.on_connect = self._on_connect
            self.client.on_disconnect = self._on_disconnect
            self.client.on_message = self._on_message

            self.client.connect(config.MQTT_BROKER, config.MQTT_PORT, 60)
            self.client.loop_start()

            logger.info(f"MQTT client started, connecting to {config.MQTT_BROKER}")
            return True

        except Exception as e:
            logger.error(f"MQTT connection failed: {e}")
            return False

    def disconnect(self):
        """Disconnect from MQTT broker."""
        if self.client:
            self.client.loop_stop()
            self.client.disconnect()
            logger.info("MQTT disconnected")

    def is_connected(self) -> bool:
        """Check if MQTT is connected."""
        return self._connected


# Global MQTT service instance
mqtt_service = MQTTService()

# =============================================================================
# FASTAPI APPLICATION
# =============================================================================

@asynccontextmanager
async def lifespan(app: FastAPI):
    """
    Application lifespan manager.

    Handles startup and shutdown events:
        - Startup: Connect to InfluxDB and MQTT
        - Shutdown: Disconnect from services
    """
    # Startup
    logger.info("Starting Heat Pump Monitor Backend...")

    # Connect to InfluxDB
    if not influxdb_service.connect():
        logger.warning("InfluxDB connection failed - some features may not work")

    # Connect to MQTT
    if not mqtt_service.connect():
        logger.warning("MQTT connection failed - real-time data unavailable")

    logger.info("Backend startup complete")

    yield

    # Shutdown
    logger.info("Shutting down...")
    mqtt_service.disconnect()
    influxdb_service.disconnect()
    logger.info("Shutdown complete")


# Create FastAPI application
app = FastAPI(
    title="Heat Pump Monitor API",
    description="""
    REST API for the Heat Pump Monitoring System.

    ## Features

    * **Real-time sensor data** from ESP32 devices via MQTT
    * **Historical data storage** in InfluxDB time-series database
    * **WebSocket support** for live dashboard updates
    * **Alert management** with threshold monitoring

    ## Architecture

    ```
    ESP32 Devices --> MQTT Broker --> This API --> InfluxDB
                                          |
                                          v
                                    Web Dashboard
    ```

    ## Authentication

    Currently no authentication is required. For production, implement
    OAuth2 or API key authentication.
    """,
    version="1.0.0",
    lifespan=lifespan,
    docs_url="/docs",
    redoc_url="/redoc",
)

# Configure CORS
app.add_middleware(
    CORSMiddleware,
    allow_origins=["*"],  # Configure appropriately for production
    allow_credentials=True,
    allow_methods=["*"],
    allow_headers=["*"],
)

# =============================================================================
# API ROUTES
# =============================================================================

@app.get("/", tags=["General"])
def root():
    """
    Root endpoint - API information.

    Returns basic API information and status.
    """
    return {
        "name": "Heat Pump Monitor API",
        "version": "1.0.0",
        "status": "running",
        "documentation": "/docs"
    }


@app.get("/api/health", response_model=HealthResponse, tags=["General"])
def health_check():
    """
    Health check endpoint.

    Returns the health status of the API and its dependencies:
    - InfluxDB connection status
    - MQTT broker connection status

    Use this endpoint for monitoring and load balancer health checks.
    """
    return HealthResponse(
        status="healthy" if influxdb_service.is_connected() else "degraded",
        influxdb_connected=influxdb_service.is_connected(),
        mqtt_connected=mqtt_service.is_connected(),
        timestamp=datetime.now(timezone.utc)
    )


@app.get("/api/stats", response_model=StatsResponse, tags=["Statistics"])
def get_stats():
    """
    Get system-wide statistics.

    Returns aggregated statistics including:
    - Total sensor readings stored
    - Total alerts recorded
    - Number of devices currently online
    - Oldest and newest reading timestamps
    """
    stats = influxdb_service.get_stats()
    return StatsResponse(**stats)


@app.get("/api/devices", tags=["Devices"])
def get_devices():
    """
    List all registered devices.

    Returns a list of all device IDs that have sent data to the system,
    along with their current online status.
    """
    device_ids = influxdb_service.get_device_list()
    devices = []

    for device_id in device_ids:
        latest = influxdb_service.get_latest_reading(device_id)

        # Device is online if we received data in last 2 minutes
        is_online = False
        last_seen = None

        if latest and latest.get("time"):
            last_seen = latest["time"]
            time_diff = datetime.now(timezone.utc) - last_seen.replace(tzinfo=timezone.utc)
            is_online = time_diff.total_seconds() < 120

        devices.append({
            "device_id": device_id,
            "is_online": is_online,
            "last_seen": last_seen.isoformat() if last_seen else None
        })

    return devices


@app.get("/api/devices/{device_id}/status", response_model=DeviceStatusResponse,
         tags=["Devices"])
def get_device_status(device_id: str):
    """
    Get current status of a specific device.

    Returns the most recent sensor readings and online status for the
    specified device.

    Args:
        device_id: Unique device identifier (e.g., "site1")

    Raises:
        HTTPException 404: If device has never sent data
    """
    latest = influxdb_service.get_latest_reading(device_id)

    if not latest:
        raise HTTPException(
            status_code=404,
            detail=f"Device '{device_id}' not found or has no data"
        )

    # Check if online (data in last 2 minutes)
    is_online = False
    if latest.get("time"):
        time_diff = datetime.now(timezone.utc) - latest["time"].replace(tzinfo=timezone.utc)
        is_online = time_diff.total_seconds() < 120

    return DeviceStatusResponse(
        device_id=device_id,
        is_online=is_online,
        last_seen=latest.get("time"),
        temp_inlet=latest.get("temp_inlet"),
        temp_outlet=latest.get("temp_outlet"),
        temp_ambient=latest.get("temp_ambient"),
        temp_compressor=latest.get("temp_compressor"),
        voltage=latest.get("voltage"),
        current=latest.get("current"),
        power=latest.get("power"),
        pressure_high=latest.get("pressure_high"),
        pressure_low=latest.get("pressure_low"),
        compressor_running=latest.get("compressor_running"),
    )


@app.get("/api/devices/{device_id}/readings", response_model=List[SensorDataResponse],
         tags=["Sensor Data"])
def get_device_readings(
    device_id: str,
    hours: int = Query(24, ge=1, le=168, description="Hours of history (1-168)"),
    limit: int = Query(100, ge=1, le=1000, description="Maximum records (1-1000)")
):
    """
    Get historical sensor readings for a device.

    Returns time-series sensor data for the specified device.
    Data is sorted by time, newest first.

    Args:
        device_id: Unique device identifier
        hours: Number of hours of history to retrieve (default: 24, max: 168)
        limit: Maximum number of records to return (default: 100, max: 1000)

    Returns:
        List of sensor readings with timestamps
    """
    readings = influxdb_service.get_readings(device_id, hours, limit)
    return [SensorDataResponse(**r) for r in readings]


@app.get("/api/devices/{device_id}/readings/latest", response_model=SensorDataResponse,
         tags=["Sensor Data"])
def get_latest_reading(device_id: str):
    """
    Get the most recent sensor reading for a device.

    Returns only the latest sensor data point. Use this for
    dashboard displays that show current values.

    Args:
        device_id: Unique device identifier

    Raises:
        HTTPException 404: If no readings found for device
    """
    reading = influxdb_service.get_latest_reading(device_id)

    if not reading:
        raise HTTPException(
            status_code=404,
            detail=f"No readings found for device '{device_id}'"
        )

    return SensorDataResponse(**reading)


@app.get("/api/alerts", response_model=List[AlertResponse], tags=["Alerts"])
def get_alerts(
    device_id: Optional[str] = Query(None, description="Filter by device ID"),
    hours: int = Query(24, ge=1, le=168, description="Hours of history"),
    limit: int = Query(50, ge=1, le=500, description="Maximum records")
):
    """
    Get alert history.

    Returns a list of alerts ordered by time (newest first).
    Can be filtered by device ID.

    Args:
        device_id: Optional filter by specific device
        hours: Hours of history to retrieve
        limit: Maximum number of alerts to return
    """
    alerts = influxdb_service.get_alerts(device_id, hours, limit)
    return [AlertResponse(**a) for a in alerts]


# =============================================================================
# WEBSOCKET ENDPOINT
# =============================================================================

@app.websocket("/ws")
async def websocket_endpoint(websocket: WebSocket):
    """
    WebSocket endpoint for real-time updates.

    Connect to this endpoint to receive live sensor data updates.

    Message types received:
        - sensor_data: New sensor reading from a device
        - alert: New alert triggered

    Message format:
    ```json
    {
        "type": "sensor_data",
        "device_id": "site1",
        "data": { ... sensor values ... },
        "timestamp": "2024-01-15T10:30:00Z"
    }
    ```

    The connection will remain open until the client disconnects.
    Send any message to receive a "pong" response (keepalive).
    """
    await ws_manager.connect(websocket)

    try:
        while True:
            # Wait for client messages (for keepalive)
            data = await websocket.receive_text()
            await websocket.send_json({
                "type": "pong",
                "timestamp": datetime.now(timezone.utc).isoformat()
            })
    except WebSocketDisconnect:
        ws_manager.disconnect(websocket)


# =============================================================================
# MAIN ENTRY POINT
# =============================================================================

if __name__ == "__main__":
    import uvicorn
    uvicorn.run(
        "main:app",
        host="0.0.0.0",
        port=8000,
        reload=True,
        log_level="info"
    )
