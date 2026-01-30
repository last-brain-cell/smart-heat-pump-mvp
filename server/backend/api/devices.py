"""
Device Endpoints
================

API endpoints for device management and sensor data retrieval.
"""

from datetime import datetime, timezone
from typing import List

from fastapi import APIRouter, HTTPException, Query

from server.backend.core.config import settings
from server.backend.schemas import DeviceResponse, DeviceStatusResponse, SensorDataResponse
from server.backend.services import influxdb_service

router = APIRouter()


@router.get("", response_model=List[DeviceResponse])
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

        # Device is online if we received data recently
        is_online = False
        last_seen = None

        if latest and latest.get("time"):
            last_seen = latest["time"]
            if hasattr(last_seen, "replace"):
                time_diff = datetime.now(timezone.utc) - last_seen.replace(
                    tzinfo=timezone.utc
                )
                is_online = (
                    time_diff.total_seconds() < settings.DEVICE_ONLINE_TIMEOUT_SECONDS
                )

        devices.append(
            DeviceResponse(
                device_id=device_id,
                is_online=is_online,
                last_seen=last_seen,
            )
        )

    return devices


@router.get("/{device_id}/status", response_model=DeviceStatusResponse)
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
            detail=f"Device '{device_id}' not found or has no data",
        )

    # Check if online (data received recently)
    is_online = False
    if latest.get("time"):
        time_diff = datetime.now(timezone.utc) - latest["time"].replace(
            tzinfo=timezone.utc
        )
        is_online = time_diff.total_seconds() < settings.DEVICE_ONLINE_TIMEOUT_SECONDS

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


@router.get("/{device_id}/readings", response_model=List[SensorDataResponse])
def get_device_readings(
    device_id: str,
    hours: int = Query(24, ge=1, le=168, description="Hours of history (1-168)"),
    limit: int = Query(100, ge=1, le=1000, description="Maximum records (1-1000)"),
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


@router.get("/{device_id}/readings/latest", response_model=SensorDataResponse)
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
            detail=f"No readings found for device '{device_id}'",
        )

    return SensorDataResponse(**reading)
