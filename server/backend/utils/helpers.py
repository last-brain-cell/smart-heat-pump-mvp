"""
Helper Functions
================

Common utility functions used across the application.
"""

from datetime import datetime, timezone
from typing import Optional

from server.backend.core.config import settings


def is_device_online(last_seen: Optional[datetime]) -> bool:
    """
    Check if a device is considered online based on last seen time.

    Args:
        last_seen: Timestamp of last data received

    Returns:
        bool: True if device is online (sent data recently)
    """
    if not last_seen:
        return False

    # Ensure timezone aware
    if last_seen.tzinfo is None:
        last_seen = last_seen.replace(tzinfo=timezone.utc)

    time_diff = datetime.now(timezone.utc) - last_seen
    return time_diff.total_seconds() < settings.DEVICE_ONLINE_TIMEOUT_SECONDS


def format_timestamp(dt: Optional[datetime]) -> Optional[str]:
    """
    Format datetime to ISO 8601 string.

    Args:
        dt: Datetime to format

    Returns:
        ISO 8601 formatted string or None
    """
    if dt is None:
        return None
    return dt.isoformat()


def parse_device_id_from_topic(topic: str) -> Optional[str]:
    """
    Extract device ID from MQTT topic.

    Args:
        topic: MQTT topic string (e.g., "heatpump/site1/data")

    Returns:
        Device ID or None if topic format is invalid
    """
    parts = topic.split("/")
    if len(parts) >= 2:
        return parts[1]
    return None
