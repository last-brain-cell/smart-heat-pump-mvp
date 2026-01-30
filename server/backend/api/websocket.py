"""
WebSocket Endpoint
==================

Real-time data streaming via WebSocket.
"""

from datetime import datetime, timezone

from fastapi import APIRouter, WebSocket, WebSocketDisconnect

from server.backend.services import ws_manager

router = APIRouter()


@router.websocket("/ws")
async def websocket_endpoint(websocket: WebSocket):
    """
    WebSocket endpoint for real-time updates.

    Connect to this endpoint to receive live sensor data updates.

    Message types received:
        - sensor_data: New sensor reading from a device
        - alert: New alert triggered

    Message format:
    ```json
    {
        "type": "sensor_data",
        "device_id": "site1",
        "data": { ... sensor values ... },
        "timestamp": "2024-01-15T10:30:00Z"
    }
    ```

    The connection will remain open until the client disconnects.
    Send any message to receive a "pong" response (keepalive).
    """
    await ws_manager.connect(websocket)

    try:
        while True:
            # Wait for client messages (for keepalive)
            data = await websocket.receive_text()
            await websocket.send_json(
                {"type": "pong", "timestamp": datetime.now(timezone.utc).isoformat()}
            )
    except WebSocketDisconnect:
        ws_manager.disconnect(websocket)
