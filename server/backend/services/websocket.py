"""
WebSocket Manager
=================

Manages WebSocket connections for real-time updates to dashboard clients.
"""

from typing import List, Dict
from fastapi import WebSocket

from server.backend.core.logging import get_logger

logger = get_logger(__name__)


class WebSocketManager:
    """
    Manages WebSocket connections for real-time updates.

    Allows the backend to broadcast sensor data updates to all
    connected dashboard clients immediately when new data arrives
    via MQTT.
    """

    def __init__(self):
        """Initialize the connection manager."""
        self.active_connections: List[WebSocket] = []

    async def connect(self, websocket: WebSocket) -> None:
        """
        Accept a new WebSocket connection.

        Args:
            websocket: The WebSocket connection to add
        """
        await websocket.accept()
        self.active_connections.append(websocket)
        logger.info(f"WebSocket connected. Total: {len(self.active_connections)}")

    def disconnect(self, websocket: WebSocket) -> None:
        """
        Remove a WebSocket connection.

        Args:
            websocket: The WebSocket connection to remove
        """
        if websocket in self.active_connections:
            self.active_connections.remove(websocket)
        logger.info(f"WebSocket disconnected. Total: {len(self.active_connections)}")

    async def broadcast(self, message: Dict) -> None:
        """
        Send a message to all connected clients.

        Args:
            message: Dictionary to send as JSON
        """
        disconnected = []

        for connection in self.active_connections:
            try:
                await connection.send_json(message)
            except Exception as e:
                logger.warning(f"Failed to send to WebSocket: {e}")
                disconnected.append(connection)

        # Clean up disconnected clients
        for conn in disconnected:
            self.disconnect(conn)

    async def send_to_client(self, websocket: WebSocket, message: Dict) -> bool:
        """
        Send a message to a specific client.

        Args:
            websocket: Target WebSocket connection
            message: Dictionary to send as JSON

        Returns:
            bool: True if send successful
        """
        try:
            await websocket.send_json(message)
            return True
        except Exception as e:
            logger.warning(f"Failed to send to WebSocket: {e}")
            return False

    @property
    def connection_count(self) -> int:
        """Get the number of active connections."""
        return len(self.active_connections)


# Global service instance
ws_manager = WebSocketManager()
