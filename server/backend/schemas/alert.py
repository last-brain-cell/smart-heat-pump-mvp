"""
Alert Schemas
=============

Pydantic models for alert records.
"""

from datetime import datetime
from typing import Optional
from pydantic import BaseModel, Field


class AlertResponse(BaseModel):
    """Alert record from the database."""

    time: datetime = Field(..., description="When the alert occurred")
    device_id: str = Field(..., description="Device that generated the alert")
    alert_type: str = Field(
        ..., description="Type of alert (voltage_high, compressor_temp, etc.)"
    )
    alert_level: str = Field(..., description="Severity level (warning, critical)")
    value: Optional[float] = Field(
        None, description="Sensor value that triggered the alert"
    )
    message: Optional[str] = Field(None, description="Alert message")

    class Config:
        json_schema_extra = {
            "example": {
                "time": "2024-01-15T10:30:00Z",
                "device_id": "site1",
                "alert_type": "voltage_high",
                "alert_level": "critical",
                "value": 255.0,
                "message": "Voltage exceeded critical threshold",
            }
        }


class AlertCreate(BaseModel):
    """Schema for creating a new alert."""

    device_id: str = Field(..., description="Device identifier")
    alert_type: str = Field(..., description="Type of alert")
    level: str = Field(..., description="Severity level")
    value: float = Field(..., description="Sensor value")
    message: Optional[str] = Field(None, description="Alert message")
