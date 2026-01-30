"""
API module - FastAPI route handlers.
"""

from fastapi import APIRouter

from .health import router as health_router
from .devices import router as devices_router
from .alerts import router as alerts_router
from .websocket import router as websocket_router

# Create main API router
api_router = APIRouter()

# Include all route modules
api_router.include_router(health_router, tags=["Health"])
api_router.include_router(devices_router, prefix="/devices", tags=["Devices"])
api_router.include_router(alerts_router, prefix="/alerts", tags=["Alerts"])
api_router.include_router(websocket_router, tags=["WebSocket"])

__all__ = ["api_router"]
