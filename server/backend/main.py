"""
Heat Pump Monitoring System - Backend API
==========================================

Main application entry point.

This module initializes and runs the FastAPI application with:
    - MQTT subscription for receiving sensor data
    - InfluxDB for time-series data storage
    - REST API endpoints for the web dashboard
    - WebSocket connections for real-time updates

Architecture:
    ESP32 Device --> MQTT Broker --> This Backend --> InfluxDB
                                          |
                                          v
                                    Web Dashboard
"""

import asyncio
from datetime import datetime, timezone
from contextlib import asynccontextmanager

from fastapi import FastAPI
from fastapi.middleware.cors import CORSMiddleware

from core import settings, setup_logging, get_logger
from api import api_router
from services import influxdb_service, mqtt_service, ws_manager

# Initialize logging
setup_logging("INFO")
logger = get_logger(__name__)


def handle_sensor_data(device_id: str, data: dict) -> None:
    """
    Handle incoming sensor data from MQTT.

    Stores data in InfluxDB and broadcasts to WebSocket clients.
    """
    # Store in InfluxDB
    influxdb_service.write_sensor_data(device_id, data)

    # Broadcast to WebSocket clients
    try:
        asyncio.run(
            ws_manager.broadcast(
                {
                    "type": "sensor_data",
                    "device_id": device_id,
                    "data": data,
                    "timestamp": datetime.now(timezone.utc).isoformat(),
                }
            )
        )
    except RuntimeError:
        # Event loop already running, use create_task instead
        pass


def handle_alert(
    device_id: str, alert_type: str, level: str, value: float, message: str
) -> None:
    """
    Handle incoming alert from MQTT.

    Stores alert in InfluxDB and broadcasts to WebSocket clients.
    """
    # Store in InfluxDB
    influxdb_service.write_alert(device_id, alert_type, level, value, message)

    # Broadcast to WebSocket clients
    try:
        asyncio.run(
            ws_manager.broadcast(
                {
                    "type": "alert",
                    "device_id": device_id,
                    "alert_type": alert_type,
                    "level": level,
                    "value": value,
                    "message": message,
                    "timestamp": datetime.now(timezone.utc).isoformat(),
                }
            )
        )
    except RuntimeError:
        pass


@asynccontextmanager
async def lifespan(app: FastAPI):
    """
    Application lifespan manager.

    Handles startup and shutdown:
        - Startup: Connect to InfluxDB and MQTT
        - Shutdown: Disconnect from services
    """
    # Startup
    logger.info("Starting Heat Pump Monitor Backend...")

    # Connect to InfluxDB
    if not influxdb_service.connect():
        logger.warning("InfluxDB connection failed - some features may not work")

    # Set MQTT handlers and connect
    mqtt_service.set_handlers(
        on_sensor_data=handle_sensor_data,
        on_alert=handle_alert,
    )
    if not mqtt_service.connect():
        logger.warning("MQTT connection failed - real-time data unavailable")

    logger.info("Backend startup complete")

    yield

    # Shutdown
    logger.info("Shutting down...")
    mqtt_service.disconnect()
    influxdb_service.disconnect()
    logger.info("Shutdown complete")


# Create FastAPI application
app = FastAPI(
    title=settings.APP_NAME,
    description="""
    REST API for the Heat Pump Monitoring System.

    ## Features

    * **Real-time sensor data** from ESP32 devices via MQTT
    * **Historical data storage** in InfluxDB time-series database
    * **WebSocket support** for live dashboard updates
    * **Alert management** with threshold monitoring

    ## Architecture

    ```
    ESP32 Devices --> MQTT Broker --> This API --> InfluxDB
                                          |
                                          v
                                    Web Dashboard
    ```
    """,
    version=settings.APP_VERSION,
    lifespan=lifespan,
    docs_url="/docs",
    redoc_url="/redoc",
)

# Configure CORS
app.add_middleware(
    CORSMiddleware,
    allow_origins=["*"],  # Configure appropriately for production
    allow_credentials=True,
    allow_methods=["*"],
    allow_headers=["*"],
)

# Include API routes
app.include_router(api_router, prefix=settings.API_PREFIX)


@app.get("/", tags=["Root"])
def root():
    """
    Root endpoint - API information.
    """
    return {
        "name": settings.APP_NAME,
        "version": settings.APP_VERSION,
        "status": "running",
        "documentation": "/docs",
    }


if __name__ == "__main__":
    import uvicorn

    uvicorn.run(
        "main:app",
        host="0.0.0.0",
        port=8000,
        reload=True,
        log_level="info",
    )
