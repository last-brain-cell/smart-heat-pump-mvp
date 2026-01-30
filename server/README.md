# Heat Pump Monitor - Server

Backend server components for the heat pump monitoring system using InfluxDB for time-series data storage.

## Why InfluxDB?

InfluxDB is optimized for IoT time-series data:
- **High write throughput** - Handles thousands of data points per second
- **Efficient compression** - Stores time-series data compactly
- **Built-in retention policies** - Automatically manages data lifecycle
- **Flux query language** - Powerful queries for aggregations and analytics
- **Downsampling** - Automatic data summarization for long-term storage

## Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                         SERVER                               │
│                                                              │
│   ┌─────────────┐    ┌─────────────┐    ┌─────────────┐    │
│   │  Mosquitto  │    │   Backend   │    │  Dashboard  │    │
│   │    MQTT     │◄──►│   FastAPI   │◄──►│   Nginx     │    │
│   │  Port 1883  │    │  Port 8000  │    │   Port 80   │    │
│   └─────────────┘    └─────────────┘    └─────────────┘    │
│         │                  │                                 │
│         │                  ▼                                 │
│         │           ┌─────────────┐                         │
│         │           │  InfluxDB   │                         │
│         └──────────►│  Port 8086  │                         │
│                     └─────────────┘                         │
│                                                              │
└─────────────────────────────────────────────────────────────┘
```

## Quick Start

### Prerequisites

- Docker and Docker Compose
- Python 3.8+ (for simulator script)

### Setup

```bash
cd server
chmod +x setup.sh
./setup.sh
```

### Access Points

| Service | URL | Description |
|---------|-----|-------------|
| Dashboard | http://localhost | Web monitoring interface |
| API | http://localhost:8000 | REST API |
| API Docs | http://localhost:8000/docs | Swagger documentation |
| InfluxDB UI | http://localhost:8086 | Database admin interface |
| MQTT | localhost:1883 | MQTT broker (for devices) |
| MQTT WebSocket | localhost:9001 | MQTT over WebSocket |

## Services

### Mosquitto MQTT Broker

Receives sensor data from ESP32 devices.

- **Port 1883**: MQTT protocol
- **Port 9001**: WebSocket (for web clients)

**Topics:**
```
heatpump/{device_id}/data       # Sensor readings (JSON)
heatpump/{device_id}/status/#   # Device status
heatpump/{device_id}/alerts     # Alert notifications
```

### InfluxDB 2.x

Time-series database for sensor data storage.

- **Port**: 8086
- **Organization**: heatpump
- **Bucket**: sensor_data
- **Retention**: 30 days (configurable)

**Measurements:**
- `sensor_reading` - Sensor data from devices
- `alert` - Alert history

**Example Flux Query:**
```flux
from(bucket: "sensor_data")
  |> range(start: -24h)
  |> filter(fn: (r) => r["device_id"] == "site1")
  |> filter(fn: (r) => r["_measurement"] == "sensor_reading")
```

### FastAPI Backend

Python REST API and MQTT subscriber.

- **Port**: 8000
- **Docs**: http://localhost:8000/docs

**Features:**
- MQTT subscription for real-time data
- InfluxDB storage and queries
- REST API for dashboard
- WebSocket for live updates

### Web Dashboard

Real-time monitoring interface.

- **Port**: 80 (via Nginx)
- Live sensor values
- Alert notifications
- Device status

## API Reference

### General

| Endpoint | Method | Description |
|----------|--------|-------------|
| `/` | GET | API information |
| `/api/health` | GET | Health check (InfluxDB, MQTT status) |
| `/api/stats` | GET | System statistics |

### Devices

| Endpoint | Method | Description |
|----------|--------|-------------|
| `/api/devices` | GET | List all devices |
| `/api/devices/{id}/status` | GET | Current device status |
| `/api/devices/{id}/readings` | GET | Historical readings |
| `/api/devices/{id}/readings/latest` | GET | Most recent reading |

### Alerts

| Endpoint | Method | Description |
|----------|--------|-------------|
| `/api/alerts` | GET | Alert history |

### WebSocket

| Endpoint | Description |
|----------|-------------|
| `/ws` | Real-time sensor updates |

## Configuration

### Environment Variables

Copy `.env.example` to `.env` and configure:

```env
# InfluxDB
INFLUXDB_USER=admin
INFLUXDB_PASSWORD=your-secure-password
INFLUXDB_ORG=heatpump
INFLUXDB_BUCKET=sensor_data
INFLUXDB_RETENTION=30d
INFLUXDB_TOKEN=your-secure-token

# MQTT
MQTT_USER=heatpump
MQTT_PASSWORD=your-secure-password
```

### ESP32 Configuration

Update `config.h` to match:

```cpp
#define MQTT_BROKER "your-server-ip"
#define MQTT_PORT 1883
#define MQTT_USER "heatpump"
#define MQTT_PASS "heatpump123"
```

## Testing

### Simulate a Device

```bash
# Install dependencies
pip install paho-mqtt

# Run simulator (sends data every 10 seconds)
python scripts/simulate_device.py --device-id site1 --interval 10
```

### Test MQTT

```bash
# Subscribe to all messages
docker exec -it heatpump-mqtt mosquitto_sub \
    -h localhost -u heatpump -P heatpump123 \
    -t 'heatpump/#' -v

# Publish test message
docker exec -it heatpump-mqtt mosquitto_pub \
    -h localhost -u heatpump -P heatpump123 \
    -t 'heatpump/test/data' \
    -m '{"temperature":{"inlet":45},"electrical":{"voltage":230}}'
```

### Query InfluxDB

```bash
# Using InfluxDB CLI
docker exec -it heatpump-influxdb influx query '
  from(bucket: "sensor_data")
    |> range(start: -1h)
    |> filter(fn: (r) => r["_measurement"] == "sensor_reading")
    |> limit(n: 10)
'
```

### View Logs

```bash
# All services
docker compose logs -f

# Specific service
docker compose logs -f backend
docker compose logs -f influxdb
docker compose logs -f mosquitto
```

## Production Deployment

### Security Checklist

1. **Change default passwords** in `.env`
2. **Generate secure InfluxDB token**
3. **Enable HTTPS** with SSL certificates
4. **Configure firewall** - only expose necessary ports
5. **Enable MQTT TLS** (port 8883)
6. **Set up backups** for InfluxDB

### Data Retention

Configure retention policies in InfluxDB:

```bash
# Default: 30 days
# Modify via INFLUXDB_RETENTION in .env
# Or use InfluxDB UI at http://localhost:8086
```

### Recommended VPS Specs

- 1-2 vCPU
- 2 GB RAM (InfluxDB benefits from memory)
- 20+ GB SSD
- Ubuntu 22.04 LTS

## File Structure

```
server/
├── docker-compose.yml      # Docker orchestration
├── .env.example            # Environment template
├── setup.sh                # Setup script
├── README.md               # This file
│
├── backend/                # FastAPI application
│   ├── Dockerfile
│   ├── requirements.txt
│   └── main.py             # API + MQTT + InfluxDB
│
├── dashboard/              # Web frontend
│   ├── index.html
│   ├── styles.css
│   └── app.js
│
├── mosquitto/              # MQTT broker
│   └── config/
│       ├── mosquitto.conf
│       └── passwd          # Generated by setup.sh
│
├── nginx/                  # Web server
│   └── nginx.conf
│
└── scripts/                # Utility scripts
    ├── requirements.txt
    └── simulate_device.py  # Device simulator
```

## Troubleshooting

### InfluxDB Issues

```bash
# Check if running
docker exec heatpump-influxdb influx ping

# View logs
docker compose logs influxdb

# Access CLI
docker exec -it heatpump-influxdb influx
```

### MQTT Connection Failed

```bash
# Check logs
docker compose logs mosquitto

# Test connection
mosquitto_pub -h localhost -u heatpump -P heatpump123 -t test -m "hello"
```

### Backend Not Starting

```bash
# Check logs
docker compose logs backend

# Restart
docker compose restart backend
```

### No Data in Dashboard

1. Check if simulator is running
2. Verify MQTT messages: `docker compose logs mosquitto`
3. Check backend logs: `docker compose logs backend`
4. Verify InfluxDB has data via UI: http://localhost:8086
