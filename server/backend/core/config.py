"""
Application Configuration
=========================

Centralized configuration management using Pydantic Settings.
All configuration is loaded from environment variables.

Environment Variables:
    INFLUXDB_URL: InfluxDB server URL
    INFLUXDB_TOKEN: Authentication token
    INFLUXDB_ORG: Organization name
    INFLUXDB_BUCKET: Bucket for sensor data
    MQTT_BROKER: MQTT broker hostname
    MQTT_PORT: MQTT broker port
    MQTT_USER: MQTT username
    MQTT_PASSWORD: MQTT password
"""

from pydantic_settings import BaseSettings
from functools import lru_cache


class Settings(BaseSettings):
    """
    Application settings loaded from environment variables.

    Uses Pydantic Settings for automatic environment variable parsing
    and validation.
    """

    # Application
    APP_NAME: str = "Heat Pump Monitor API"
    APP_VERSION: str = "1.0.0"
    DEBUG: bool = False

    # InfluxDB Configuration
    INFLUXDB_URL: str = "http://localhost:8086"
    INFLUXDB_TOKEN: str = "heatpump-super-secret-token"
    INFLUXDB_ORG: str = "heatpump"
    INFLUXDB_BUCKET: str = "sensor_data"

    # MQTT Configuration
    MQTT_BROKER: str = "localhost"
    MQTT_PORT: int = 1883
    MQTT_USER: str = "heatpump"
    MQTT_PASSWORD: str = "heatpump123"

    # MQTT Topics
    MQTT_TOPIC_DATA: str = "heatpump/+/data"
    MQTT_TOPIC_STATUS: str = "heatpump/+/status/#"
    MQTT_TOPIC_ALERTS: str = "heatpump/+/alerts"

    # Device Settings
    DEVICE_ONLINE_TIMEOUT_SECONDS: int = 120  # Consider offline after 2 minutes

    # API Settings
    API_PREFIX: str = "/api"

    class Config:
        env_file = ".env"
        case_sensitive = True


@lru_cache()
def get_settings() -> Settings:
    """
    Get cached settings instance.

    Uses LRU cache to avoid reloading settings on every access.

    Returns:
        Settings: Application settings
    """
    return Settings()


# Global settings instance
settings = get_settings()
