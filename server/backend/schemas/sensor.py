"""
Sensor Data Schemas
===================

Pydantic models for sensor readings from heat pump devices.
"""

from datetime import datetime
from typing import Optional, Dict
from pydantic import BaseModel, Field


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


class PhaseReading(BaseModel):
    """Single phase electrical data from PZEM-004T."""
    voltage: Optional[float] = Field(None, description="AC voltage (V)")
    current: Optional[float] = Field(None, description="Current (A)")
    power: Optional[float] = Field(None, description="Power (W)")
    energy: Optional[float] = Field(None, description="Cumulative energy (kWh)")
    frequency: Optional[float] = Field(None, description="Frequency (Hz)")
    power_factor: Optional[float] = Field(None, description="Power factor (0-1)")


class ElectricalReadingV2(BaseModel):
    """3-phase electrical readings from v2 firmware."""
    phase1: Optional[PhaseReading] = None
    phase2: Optional[PhaseReading] = None
    phase3: Optional[PhaseReading] = None


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
    alerts: Optional[Dict[str, int]] = Field(
        None, description="Alert levels (0=OK, 1=Warning, 2=Critical)"
    )


class SensorDataResponse(BaseModel):
    """Response model for sensor readings from the database."""

    time: datetime = Field(..., description="Timestamp of the reading")
    device_id: str = Field(..., description="Device identifier")
    firmware_version: Optional[str] = Field(None, description="Firmware version (v1 or v2)")
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

    # Per-phase fields (v2 firmware, 3-phase PZEM-004T)
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
                "time": "2024-01-15T10:30:00Z",
                "device_id": "site1",
                "firmware_version": "v1",
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
