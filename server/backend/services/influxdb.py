"""
InfluxDB Service
================

Service class for InfluxDB operations including:
- Writing sensor data points
- Querying historical data
- Getting device status
- Retrieving statistics

Uses the InfluxDB 2.x Python client with Flux query language.
"""

from typing import Optional, List, Dict, Any

from influxdb_client import InfluxDBClient, Point, WritePrecision
from influxdb_client.client.write_api import SYNCHRONOUS

from core.config import settings
from core.logging import get_logger

logger = get_logger(__name__)


class InfluxDBService:
    """
    Service class for all InfluxDB operations.

    Provides a clean interface for storing and querying time-series data.
    """

    def __init__(self):
        """Initialize InfluxDB service (not connected yet)."""
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
                url=settings.INFLUXDB_URL,
                token=settings.INFLUXDB_TOKEN,
                org=settings.INFLUXDB_ORG,
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

    def disconnect(self) -> None:
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
        except Exception:
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
                bucket=settings.INFLUXDB_BUCKET,
                org=settings.INFLUXDB_ORG,
                record=point,
                write_precision=WritePrecision.MS,
            )

            logger.debug(f"Wrote sensor data for device {device_id}")
            return True

        except Exception as e:
            logger.error(f"Failed to write sensor data: {e}")
            return False

    def write_alert(
        self,
        device_id: str,
        alert_type: str,
        level: str,
        value: float,
        message: Optional[str] = None,
    ) -> bool:
        """
        Write an alert record to InfluxDB.

        Args:
            device_id: Device that generated the alert
            alert_type: Type of alert (e.g., "voltage_high")
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
                bucket=settings.INFLUXDB_BUCKET,
                org=settings.INFLUXDB_ORG,
                record=point,
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
        from(bucket: "{settings.INFLUXDB_BUCKET}")
            |> range(start: -24h)
            |> filter(fn: (r) => r["_measurement"] == "sensor_reading")
            |> filter(fn: (r) => r["device_id"] == "{device_id}")
            |> last()
            |> pivot(rowKey: ["_time"], columnKey: ["_field"], valueColumn: "_value")
        '''

        try:
            tables = self.query_api.query(query, org=settings.INFLUXDB_ORG)

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

    def get_readings(
        self, device_id: str, hours: int = 24, limit: int = 100
    ) -> List[Dict]:
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
        from(bucket: "{settings.INFLUXDB_BUCKET}")
            |> range(start: -{hours}h)
            |> filter(fn: (r) => r["_measurement"] == "sensor_reading")
            |> filter(fn: (r) => r["device_id"] == "{device_id}")
            |> pivot(rowKey: ["_time"], columnKey: ["_field"], valueColumn: "_value")
            |> sort(columns: ["_time"], desc: true)
            |> limit(n: {limit})
        '''

        try:
            tables = self.query_api.query(query, org=settings.INFLUXDB_ORG)
            readings = []

            for table in tables:
                for record in table.records:
                    readings.append(
                        {
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
                    )

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
        from(bucket: "{settings.INFLUXDB_BUCKET}")
            |> range(start: -30d)
            |> filter(fn: (r) => r["_measurement"] == "sensor_reading")
            |> keep(columns: ["device_id"])
            |> distinct(column: "device_id")
        '''

        try:
            tables = self.query_api.query(query, org=settings.INFLUXDB_ORG)
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
            from(bucket: "{settings.INFLUXDB_BUCKET}")
                |> range(start: -30d)
                |> filter(fn: (r) => r["_measurement"] == "sensor_reading")
                |> count()
                |> sum()
            '''
            tables = self.query_api.query(query, org=settings.INFLUXDB_ORG)
            for table in tables:
                for record in table.records:
                    stats["total_readings"] = int(record.get_value() or 0)

            # Count alerts
            query = f'''
            from(bucket: "{settings.INFLUXDB_BUCKET}")
                |> range(start: -30d)
                |> filter(fn: (r) => r["_measurement"] == "alert")
                |> count()
            '''
            tables = self.query_api.query(query, org=settings.INFLUXDB_ORG)
            for table in tables:
                for record in table.records:
                    stats["total_alerts"] += int(record.get_value() or 0)

            # Count online devices (active in last 5 minutes)
            query = f'''
            from(bucket: "{settings.INFLUXDB_BUCKET}")
                |> range(start: -5m)
                |> filter(fn: (r) => r["_measurement"] == "sensor_reading")
                |> keep(columns: ["device_id"])
                |> distinct(column: "device_id")
                |> count()
            '''
            tables = self.query_api.query(query, org=settings.INFLUXDB_ORG)
            for table in tables:
                for record in table.records:
                    stats["devices_online"] = int(record.get_value() or 0)

        except Exception as e:
            logger.error(f"Failed to get stats: {e}")

        return stats

    def get_alerts(
        self, device_id: Optional[str] = None, hours: int = 24, limit: int = 50
    ) -> List[Dict]:
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
        from(bucket: "{settings.INFLUXDB_BUCKET}")
            |> range(start: -{hours}h)
            |> filter(fn: (r) => r["_measurement"] == "alert")
            {device_filter}
            |> pivot(rowKey: ["_time"], columnKey: ["_field"], valueColumn: "_value")
            |> sort(columns: ["_time"], desc: true)
            |> limit(n: {limit})
        '''

        try:
            tables = self.query_api.query(query, org=settings.INFLUXDB_ORG)
            alerts = []

            for table in tables:
                for record in table.records:
                    alerts.append(
                        {
                            "time": record.get_time(),
                            "device_id": record.values.get("device_id"),
                            "alert_type": record.values.get("alert_type"),
                            "alert_level": record.values.get("level"),
                            "value": record.values.get("value"),
                            "message": record.values.get("message"),
                        }
                    )

            return alerts

        except Exception as e:
            logger.error(f"Failed to get alerts: {e}")
            return []


# Global service instance
influxdb_service = InfluxDBService()
