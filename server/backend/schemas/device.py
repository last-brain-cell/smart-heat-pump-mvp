"""
Device Schemas
==============

Pydantic models for device information and status.
"""

from datetime import datetime
from typing import Optional
from pydantic import BaseModel, Field


class DeviceResponse(BaseModel):
    """Basic device information."""

    device_id: str = Field(..., description="Unique device identifier")
    is_online: bool = Field(..., description="Whether device is currently online")
    last_seen: Optional[datetime] = Field(None, description="Last data received timestamp")


class DeviceStatusResponse(BaseModel):
    """Complete device status with latest sensor readings."""

    device_id: str = Field(..., description="Device identifier")
    is_online: bool = Field(..., description="Whether device has sent data recently")
    last_seen: Optional[datetime] = Field(None, description="Last data received timestamp")

    # Temperature readings
    temp_inlet: Optional[float] = Field(None, description="Inlet temperature (°C)")
    temp_outlet: Optional[float] = Field(None, description="Outlet temperature (°C)")
    temp_ambient: Optional[float] = Field(None, description="Ambient temperature (°C)")
    temp_compressor: Optional[float] = Field(None, description="Compressor temperature (°C)")

    # Electrical readings
    voltage: Optional[float] = Field(None, description="Voltage (V)")
    current: Optional[float] = Field(None, description="Current (A)")
    power: Optional[float] = Field(None, description="Power (W)")

    # Pressure readings
    pressure_high: Optional[float] = Field(None, description="High side pressure (PSI)")
    pressure_low: Optional[float] = Field(None, description="Low side pressure (PSI)")

    # Status
    compressor_running: Optional[bool] = Field(None, description="Compressor state")

    # Firmware version
    firmware_version: Optional[str] = Field(None, description="Firmware version (v1 or v2)")

    # Per-phase electrical readings (v2 firmware, 3-phase PZEM-004T)
    phase1_voltage: Optional[float] = None
    phase1_current: Optional[float] = None
    phase1_power: Optional[float] = None
    phase1_energy: Optional[float] = None
    phase1_frequency: Optional[float] = None
    phase1_pf: Optional[float] = None

    phase2_voltage: Optional[float] = None
    phase2_current: Optional[float] = None
    phase2_power: Optional[float] = None
    phase2_energy: Optional[float] = None
    phase2_frequency: Optional[float] = None
    phase2_pf: Optional[float] = None

    phase3_voltage: Optional[float] = None
    phase3_current: Optional[float] = None
    phase3_power: Optional[float] = None
    phase3_energy: Optional[float] = None
    phase3_frequency: Optional[float] = None
    phase3_pf: Optional[float] = None

    class Config:
        json_schema_extra = {
            "example": {
                "device_id": "site1",
                "is_online": True,
                "last_seen": "2024-01-15T10:30:00Z",
                "temp_inlet": 45.2,
                "temp_outlet": 50.1,
                "temp_ambient": 25.0,
                "temp_compressor": 70.5,
                "voltage": 230.5,
                "current": 8.5,
                "power": 1959.25,
                "pressure_high": 280.0,
                "pressure_low": 70.0,
                "compressor_running": True,
            }
        }
