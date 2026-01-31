"""
Health & Stats Endpoints
========================

API endpoints for health checks and system statistics.
"""

from datetime import datetime, timezone

from fastapi import APIRouter

from schemas import HealthResponse, StatsResponse
from services import influxdb_service, mqtt_service

router = APIRouter()


@router.get("/health", response_model=HealthResponse)
def health_check():
    """
    Health check endpoint.

    Returns the health status of the API and its dependencies:
    - InfluxDB connection status
    - MQTT broker connection status

    Use this endpoint for monitoring and load balancer health checks.
    """
    influxdb_ok = influxdb_service.is_connected()
    mqtt_ok = mqtt_service.is_connected()

    if influxdb_ok and mqtt_ok:
        status = "healthy"
    elif influxdb_ok:
        status = "degraded"
    else:
        status = "unhealthy"

    return HealthResponse(
        status=status,
        influxdb_connected=influxdb_ok,
        mqtt_connected=mqtt_ok,
        timestamp=datetime.now(timezone.utc),
    )


@router.get("/stats", response_model=StatsResponse)
def get_stats():
    """
    Get system-wide statistics.

    Returns aggregated statistics including:
    - Total sensor readings stored
    - Total alerts recorded
    - Number of devices currently online
    """
    stats = influxdb_service.get_stats()
    return StatsResponse(**stats)
