"""
Alert Endpoints
===============

API endpoints for alert management.
"""

from typing import List, Optional

from fastapi import APIRouter, Query

from schemas import AlertResponse
from services import influxdb_service

router = APIRouter()


@router.get("", response_model=List[AlertResponse])
def get_alerts(
    device_id: Optional[str] = Query(None, description="Filter by device ID"),
    hours: int = Query(24, ge=1, le=168, description="Hours of history"),
    limit: int = Query(50, ge=1, le=500, description="Maximum records"),
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
