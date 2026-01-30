"""
Common Schemas
==============

Shared Pydantic models used across the application.
"""

from datetime import datetime
from typing import Optional
from pydantic import BaseModel, Field


class HealthResponse(BaseModel):
    """API health check response."""

    status: str = Field(..., description="Health status (healthy/degraded/unhealthy)")
    influxdb_connected: bool = Field(..., description="InfluxDB connection status")
    mqtt_connected: bool = Field(..., description="MQTT broker connection status")
    timestamp: datetime = Field(..., description="Health check timestamp")

    class Config:
        json_schema_extra = {
            "example": {
                "status": "healthy",
                "influxdb_connected": True,
                "mqtt_connected": True,
                "timestamp": "2024-01-15T10:30:00Z",
            }
        }


class StatsResponse(BaseModel):
    """System-wide statistics."""

    total_readings: int = Field(..., description="Total sensor readings in database")
    total_alerts: int = Field(..., description="Total alerts recorded")
    devices_online: int = Field(..., description="Number of devices online")
    oldest_reading: Optional[datetime] = Field(
        None, description="Oldest reading timestamp"
    )
    newest_reading: Optional[datetime] = Field(
        None, description="Most recent reading timestamp"
    )

    class Config:
        json_schema_extra = {
            "example": {
                "total_readings": 10000,
                "total_alerts": 25,
                "devices_online": 3,
                "oldest_reading": "2024-01-01T00:00:00Z",
                "newest_reading": "2024-01-15T10:30:00Z",
            }
        }


class MessageResponse(BaseModel):
    """Generic message response."""

    message: str = Field(..., description="Response message")
    success: bool = Field(default=True, description="Operation success status")
