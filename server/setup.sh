#!/bin/bash
# =============================================================================
# Heat Pump Monitor - Server Setup Script
# =============================================================================
#
# This script sets up the complete server environment including:
#   - Mosquitto MQTT Broker
#   - InfluxDB Time-Series Database
#   - FastAPI Backend
#   - Nginx Dashboard
#
# Usage:
#   chmod +x setup.sh
#   ./setup.sh
#
# =============================================================================

set -e

echo "========================================"
echo "  Heat Pump Monitor - Server Setup"
echo "========================================"
echo ""

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# -----------------------------------------------------------------------------
# Check Prerequisites
# -----------------------------------------------------------------------------

echo -e "${BLUE}Checking prerequisites...${NC}"

# Check if Docker is installed
if ! command -v docker &> /dev/null; then
    echo -e "${RED}Error: Docker is not installed${NC}"
    echo "Please install Docker first: https://docs.docker.com/get-docker/"
    exit 1
fi

# Check if Docker Compose is available
if ! docker compose version &> /dev/null; then
    if ! command -v docker-compose &> /dev/null; then
        echo -e "${RED}Error: Docker Compose is not installed${NC}"
        echo "Please install Docker Compose: https://docs.docker.com/compose/install/"
        exit 1
    fi
    COMPOSE_CMD="docker-compose"
else
    COMPOSE_CMD="docker compose"
fi

echo -e "${GREEN}Docker and Docker Compose found${NC}"
echo ""

# -----------------------------------------------------------------------------
# Create Environment File
# -----------------------------------------------------------------------------

echo -e "${BLUE}Setting up environment...${NC}"

if [ ! -f .env ]; then
    echo "Creating .env file from template..."
    cp .env.example .env
    echo -e "${GREEN}Created .env file${NC}"
    echo -e "${YELLOW}NOTE: Update .env with secure passwords for production!${NC}"
else
    echo -e "${GREEN}.env file already exists${NC}"
fi

# Load environment variables
source .env 2>/dev/null || true

# -----------------------------------------------------------------------------
# Create Directories
# -----------------------------------------------------------------------------

echo ""
echo -e "${BLUE}Creating directories...${NC}"

mkdir -p mosquitto/config mosquitto/data mosquitto/log
chmod -R 755 mosquitto

echo -e "${GREEN}Directories created${NC}"

# -----------------------------------------------------------------------------
# Setup Mosquitto MQTT Broker
# -----------------------------------------------------------------------------

echo ""
echo -e "${BLUE}Setting up Mosquitto MQTT authentication...${NC}"

MQTT_USER=${MQTT_USER:-heatpump}
MQTT_PASS=${MQTT_PASSWORD:-heatpump123}

# Generate password file using mosquitto_passwd
echo "Generating MQTT password file..."
docker run --rm -v "$(pwd)/mosquitto/config:/mosquitto/config" \
    eclipse-mosquitto:2 \
    mosquitto_passwd -b -c /mosquitto/config/passwd "$MQTT_USER" "$MQTT_PASS" 2>/dev/null

echo -e "${GREEN}MQTT credentials configured${NC}"

# -----------------------------------------------------------------------------
# Build and Start Services
# -----------------------------------------------------------------------------

echo ""
echo "========================================"
echo -e "${BLUE}  Building and Starting Services${NC}"
echo "========================================"
echo ""

# Stop any existing containers
$COMPOSE_CMD down --remove-orphans 2>/dev/null || true

# Build images
echo "Building Docker images..."
$COMPOSE_CMD build

# Start services
echo "Starting services..."
$COMPOSE_CMD up -d

# -----------------------------------------------------------------------------
# Wait for Services to Initialize
# -----------------------------------------------------------------------------

echo ""
echo "Waiting for services to initialize..."

# Wait for InfluxDB
echo -n "  InfluxDB: "
for i in {1..30}; do
    if docker exec heatpump-influxdb influx ping &>/dev/null; then
        echo -e "${GREEN}Ready${NC}"
        break
    fi
    echo -n "."
    sleep 2
done

# Wait for Backend
echo -n "  Backend:  "
for i in {1..30}; do
    if curl -s http://localhost:8000/api/health &>/dev/null; then
        echo -e "${GREEN}Ready${NC}"
        break
    fi
    echo -n "."
    sleep 2
done

# Wait for Dashboard
echo -n "  Dashboard: "
for i in {1..10}; do
    if curl -s http://localhost &>/dev/null; then
        echo -e "${GREEN}Ready${NC}"
        break
    fi
    echo -n "."
    sleep 1
done

# -----------------------------------------------------------------------------
# Show Status
# -----------------------------------------------------------------------------

echo ""
echo "========================================"
echo -e "${BLUE}  Service Status${NC}"
echo "========================================"
echo ""

$COMPOSE_CMD ps

# -----------------------------------------------------------------------------
# Show Access Information
# -----------------------------------------------------------------------------

echo ""
echo "========================================"
echo -e "${GREEN}  Setup Complete!${NC}"
echo "========================================"
echo ""
echo -e "Access Points:"
echo -e "  ${BLUE}Dashboard:${NC}     http://localhost"
echo -e "  ${BLUE}API:${NC}           http://localhost:8000"
echo -e "  ${BLUE}API Docs:${NC}      http://localhost:8000/docs"
echo -e "  ${BLUE}InfluxDB UI:${NC}   http://localhost:8086"
echo -e "  ${BLUE}MQTT Broker:${NC}   localhost:1883"
echo -e "  ${BLUE}MQTT WebSocket:${NC} localhost:9001"
echo ""
echo -e "Default Credentials:"
echo -e "  ${YELLOW}MQTT:${NC}"
echo -e "    Username: $MQTT_USER"
echo -e "    Password: $MQTT_PASS"
echo ""
echo -e "  ${YELLOW}InfluxDB:${NC}"
echo -e "    Username: ${INFLUXDB_USER:-admin}"
echo -e "    Password: ${INFLUXDB_PASSWORD:-heatpump123}"
echo -e "    Org:      ${INFLUXDB_ORG:-heatpump}"
echo -e "    Bucket:   ${INFLUXDB_BUCKET:-sensor_data}"
echo -e "    Token:    ${INFLUXDB_TOKEN:-heatpump-super-secret-token}"
echo ""
echo -e "${RED}IMPORTANT: Change these credentials in .env for production!${NC}"
echo ""
echo -e "Useful Commands:"
echo -e "  View logs:     ${BLUE}$COMPOSE_CMD logs -f${NC}"
echo -e "  Stop:          ${BLUE}$COMPOSE_CMD down${NC}"
echo -e "  Restart:       ${BLUE}$COMPOSE_CMD restart${NC}"
echo -e "  View status:   ${BLUE}$COMPOSE_CMD ps${NC}"
echo ""
echo -e "Test with device simulator:"
echo -e "  ${BLUE}pip install paho-mqtt${NC}"
echo -e "  ${BLUE}python scripts/simulate_device.py${NC}"
echo ""
