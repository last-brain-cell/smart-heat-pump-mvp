"""
Schemas module - Pydantic models for request/response validation.
"""

from .sensor import (
    TemperatureReading,
    ElectricalReading,
    PressureReading,
    SystemStatus,
    SensorDataPayload,
    SensorDataResponse,
)
from .device import DeviceResponse, DeviceStatusResponse
from .alert import AlertResponse
from .common import HealthResponse, StatsResponse

__all__ = [
    # Sensor schemas
    "TemperatureReading",
    "ElectricalReading",
    "PressureReading",
    "SystemStatus",
    "SensorDataPayload",
    "SensorDataResponse",
    # Device schemas
    "DeviceResponse",
    "DeviceStatusResponse",
    # Alert schemas
    "AlertResponse",
    # Common schemas
    "HealthResponse",
    "StatsResponse",
]
