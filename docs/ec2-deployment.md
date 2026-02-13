# Deploy Server to AWS EC2 with CI/CD

## Context

The `server/` directory runs locally via Docker Compose with bind-mounted configs. We want to deploy on AWS EC2 with CI/CD. The server should have **zero source code** — just Docker pulling images from GHCR and running them.

**Approach:** Build 3 Docker images (backend, dashboard, mqtt) in GitHub Actions, push to GHCR. The CI/CD workflow SSHs into EC2, writes a compose file + .env, and runs `docker compose pull && up`. No git clone, no bind mounts, no source code on the server.

---

## AWS EC2 Setup (Manual, One-Time)

**Instance:** `t3.micro` (free tier) or `t3.small` (~$15/mo)
**AMI:** Ubuntu 24.04 LTS | **Storage:** 20 GB gp3
**Elastic IP:** Allocate one (free while instance running)

**Security Group:**

| Port | Source        | Purpose             |
|------|--------------|---------------------|
| 22   | Your IP only | SSH                 |
| 80   | 0.0.0.0/0   | Dashboard           |
| 1883 | 0.0.0.0/0   | MQTT (ESP32 devices)|

Ports 8000 (FastAPI), 8086 (InfluxDB), 9001 (MQTT WS) stay internal — Nginx proxies API/WS traffic, InfluxDB is accessed via SSH tunnel if needed.

**First-time setup** (SSH in):
```bash
# Install Docker
sudo apt update && sudo apt install -y ca-certificates curl gnupg
sudo install -m 0755 -d /etc/apt/keyrings
curl -fsSL https://download.docker.com/linux/ubuntu/gpg | sudo gpg --dearmor -o /etc/apt/keyrings/docker.gpg
echo "deb [arch=$(dpkg --print-architecture) signed-by=/etc/apt/keyrings/docker.gpg] https://download.docker.com/linux/ubuntu $(lsb_release -cs) stable" | sudo tee /etc/apt/sources.list.d/docker.list
sudo apt update && sudo apt install -y docker-ce docker-ce-cli containerd.io docker-compose-plugin
sudo usermod -aG docker ubuntu && newgrp docker

# Auth with GHCR (GitHub PAT with read:packages scope)
echo "<GITHUB_PAT>" | docker login ghcr.io -u <github-username> --password-stdin

# Create deploy directory
mkdir -p ~/heatpump
```

That's it. The first CI/CD run writes the compose file and .env, then pulls and starts everything.

---

## GHCR Images (Built by CI/CD)

| Image | Source | What's baked in |
|-------|--------|-----------------|
| `heatpump-backend` | `server/backend/Dockerfile` | FastAPI app + uvicorn |
| `heatpump-dashboard` | `server/dashboard/Dockerfile` | Nginx + HTML/CSS/JS + nginx.conf |
| `heatpump-mqtt` | `server/mosquitto/Dockerfile` | Mosquitto + config + entrypoint that generates passwd from env vars |
| `influxdb:2.7-alpine` | Stock image | Configured via env vars, no custom build needed |

---

## Files to Create

### 1. `server/mosquitto/Dockerfile` — Custom MQTT image

Bakes in the config and generates the passwd file from env vars at startup.

```dockerfile
FROM eclipse-mosquitto:2

COPY config/mosquitto.conf /mosquitto/config/mosquitto.conf

COPY entrypoint.sh /entrypoint.sh
RUN chmod +x /entrypoint.sh

ENTRYPOINT ["/entrypoint.sh"]
CMD ["/usr/sbin/mosquitto", "-c", "/mosquitto/config/mosquitto.conf"]
```

### 2. `server/mosquitto/entrypoint.sh` — Generates passwd from env vars

```bash
#!/bin/sh
set -e

if [ -n "$MQTT_USER" ] && [ -n "$MQTT_PASSWORD" ]; then
  mosquitto_passwd -b -c /mosquitto/config/passwd "$MQTT_USER" "$MQTT_PASSWORD"
fi

exec "$@"
```

### 3. `server/dashboard/Dockerfile` — Nginx with dashboard baked in

```dockerfile
FROM nginx:alpine

COPY nginx.conf /etc/nginx/conf.d/default.conf
COPY index.html /usr/share/nginx/html/index.html
COPY styles.css /usr/share/nginx/html/styles.css
COPY app.js /usr/share/nginx/html/app.js
```

Requires moving `server/nginx/nginx.conf` into `server/dashboard/` so it's in the same build context.

### 4. `.github/workflows/deploy.yml` — CI/CD pipeline

Two jobs: **build** (builds 3 images, pushes to GHCR) then **deploy** (SSHs into EC2, writes compose + .env, pulls and runs).

```yaml
name: Deploy to EC2

on:
  push:
    branches: [main]
    paths: ['server/**']
  workflow_dispatch:

env:
  REGISTRY: ghcr.io
  OWNER: ${{ github.repository_owner }}

jobs:
  build:
    name: Build & Push Images
    runs-on: ubuntu-latest
    permissions:
      contents: read
      packages: write
    steps:
      - uses: actions/checkout@v4

      - name: Log in to GHCR
        uses: docker/login-action@v3
        with:
          registry: ghcr.io
          username: ${{ github.actor }}
          password: ${{ secrets.GITHUB_TOKEN }}

      - name: Build and push backend
        uses: docker/build-push-action@v6
        with:
          context: server/backend
          push: true
          tags: |
            ${{ env.REGISTRY }}/${{ env.OWNER }}/heatpump-backend:latest
            ${{ env.REGISTRY }}/${{ env.OWNER }}/heatpump-backend:${{ github.sha }}

      - name: Build and push dashboard
        uses: docker/build-push-action@v6
        with:
          context: server/dashboard
          push: true
          tags: |
            ${{ env.REGISTRY }}/${{ env.OWNER }}/heatpump-dashboard:latest
            ${{ env.REGISTRY }}/${{ env.OWNER }}/heatpump-dashboard:${{ github.sha }}

      - name: Build and push mosquitto
        uses: docker/build-push-action@v6
        with:
          context: server/mosquitto
          push: true
          tags: |
            ${{ env.REGISTRY }}/${{ env.OWNER }}/heatpump-mqtt:latest
            ${{ env.REGISTRY }}/${{ env.OWNER }}/heatpump-mqtt:${{ github.sha }}

  deploy:
    name: Deploy to EC2
    needs: build
    runs-on: ubuntu-latest
    timeout-minutes: 10
    steps:
      - name: Deploy via SSH
        uses: appleboy/ssh-action@v1.0.3
        with:
          host: ${{ secrets.EC2_HOST }}
          username: ubuntu
          key: ${{ secrets.EC2_SSH_KEY }}
          port: 22
          script_stop: true
          envs: INFLUXDB_USER,INFLUXDB_PASSWORD,INFLUXDB_ORG,INFLUXDB_BUCKET,INFLUXDB_RETENTION,INFLUXDB_TOKEN,MQTT_USER,MQTT_PASSWORD,OWNER
          script: |
            set -e
            cd ~/heatpump

            # Write .env
            cat > .env << EOF
            INFLUXDB_USER=${INFLUXDB_USER}
            INFLUXDB_PASSWORD=${INFLUXDB_PASSWORD}
            INFLUXDB_ORG=${INFLUXDB_ORG}
            INFLUXDB_BUCKET=${INFLUXDB_BUCKET}
            INFLUXDB_RETENTION=${INFLUXDB_RETENTION}
            INFLUXDB_TOKEN=${INFLUXDB_TOKEN}
            MQTT_USER=${MQTT_USER}
            MQTT_PASSWORD=${MQTT_PASSWORD}
            EOF

            # Write docker-compose.yml (all images from GHCR, no bind mounts)
            cat > docker-compose.yml << 'COMPOSEOF'
            services:
              mosquitto:
                image: ghcr.io/OWNER_PLACEHOLDER/heatpump-mqtt:latest
                container_name: heatpump-mqtt
                restart: unless-stopped
                ports:
                  - "1883:1883"
                  - "9001:9001"
                environment:
                  - MQTT_USER=${MQTT_USER}
                  - MQTT_PASSWORD=${MQTT_PASSWORD}
                volumes:
                  - mqtt_data:/mosquitto/data
                  - mqtt_log:/mosquitto/log
                networks:
                  - heatpump

              influxdb:
                image: influxdb:2.7-alpine
                container_name: heatpump-influxdb
                restart: unless-stopped
                environment:
                  - DOCKER_INFLUXDB_INIT_MODE=setup
                  - DOCKER_INFLUXDB_INIT_USERNAME=${INFLUXDB_USER}
                  - DOCKER_INFLUXDB_INIT_PASSWORD=${INFLUXDB_PASSWORD}
                  - DOCKER_INFLUXDB_INIT_ORG=${INFLUXDB_ORG}
                  - DOCKER_INFLUXDB_INIT_BUCKET=${INFLUXDB_BUCKET}
                  - DOCKER_INFLUXDB_INIT_RETENTION=${INFLUXDB_RETENTION}
                  - DOCKER_INFLUXDB_INIT_ADMIN_TOKEN=${INFLUXDB_TOKEN}
                volumes:
                  - influxdb_data:/var/lib/influxdb2
                  - influxdb_config:/etc/influxdb2
                networks:
                  - heatpump
                healthcheck:
                  test: ["CMD", "influx", "ping"]
                  interval: 10s
                  timeout: 5s
                  retries: 5

              backend:
                image: ghcr.io/OWNER_PLACEHOLDER/heatpump-backend:latest
                container_name: heatpump-backend
                restart: unless-stopped
                environment:
                  - INFLUXDB_URL=http://influxdb:8086
                  - INFLUXDB_TOKEN=${INFLUXDB_TOKEN}
                  - INFLUXDB_ORG=${INFLUXDB_ORG}
                  - INFLUXDB_BUCKET=${INFLUXDB_BUCKET}
                  - MQTT_BROKER=mosquitto
                  - MQTT_PORT=1883
                  - MQTT_USER=${MQTT_USER}
                  - MQTT_PASSWORD=${MQTT_PASSWORD}
                depends_on:
                  influxdb:
                    condition: service_healthy
                  mosquitto:
                    condition: service_started
                networks:
                  - heatpump
                healthcheck:
                  test: ["CMD", "curl", "-f", "http://localhost:8000/api/health"]
                  interval: 5s
                  timeout: 5s
                  retries: 10
                  start_period: 10s

              dashboard:
                image: ghcr.io/OWNER_PLACEHOLDER/heatpump-dashboard:latest
                container_name: heatpump-dashboard
                restart: unless-stopped
                ports:
                  - "80:80"
                depends_on:
                  backend:
                    condition: service_healthy
                networks:
                  - heatpump

            networks:
              heatpump:
                driver: bridge

            volumes:
              mqtt_data:
              mqtt_log:
              influxdb_data:
              influxdb_config:
            COMPOSEOF

            # Replace owner placeholder
            sed -i "s/OWNER_PLACEHOLDER/${OWNER}/g" docker-compose.yml

            # Pull and restart
            docker compose pull
            docker compose up -d

            # Verify
            sleep 15
            curl -f http://localhost/api/health || echo "WARNING: Health check failed"
            docker image prune -f
        env:
          INFLUXDB_USER: ${{ secrets.INFLUXDB_USER }}
          INFLUXDB_PASSWORD: ${{ secrets.INFLUXDB_PASSWORD }}
          INFLUXDB_ORG: ${{ secrets.INFLUXDB_ORG }}
          INFLUXDB_BUCKET: ${{ secrets.INFLUXDB_BUCKET }}
          INFLUXDB_RETENTION: ${{ secrets.INFLUXDB_RETENTION }}
          INFLUXDB_TOKEN: ${{ secrets.INFLUXDB_TOKEN }}
          MQTT_USER: ${{ secrets.MQTT_USER }}
          MQTT_PASSWORD: ${{ secrets.MQTT_PASSWORD }}
          OWNER: ${{ github.repository_owner }}
```

**How it works:**
1. **Build job:** Builds 3 Docker images on GitHub runners, pushes to GHCR tagged `latest` + commit SHA.
2. **Deploy job:** SSHs into EC2, writes `docker-compose.yml` + `.env`, runs `docker compose pull && up -d`.

No git, no source code, no building on EC2. It just pulls pre-built images and runs them.

---

## Files to Modify

### 5. `server/backend/Dockerfile` (line 27)

Remove `--reload` (dev-only):
```
- CMD ["uvicorn", "main:app", "--host", "0.0.0.0", "--port", "8000", "--reload"]
+ CMD ["uvicorn", "main:app", "--host", "0.0.0.0", "--port", "8000"]
```

### 6. `server/nginx/nginx.conf` (line 6)

Change to catch-all for EC2 IP:
```
- server_name localhost;
+ server_name _;
```

### 7. `firmware/config.h` (line 65) — After EC2 is up

```
- #define MQTT_BROKER "192.168.1.7"
+ #define MQTT_BROKER "<your-elastic-ip>"
```

---

## GitHub Secrets to Configure

Go to repo **Settings > Secrets and variables > Actions** and add:

| Secret              | Value                              |
|---------------------|------------------------------------|
| `EC2_HOST`          | Elastic IP                         |
| `EC2_SSH_KEY`       | EC2 `.pem` key contents            |
| `INFLUXDB_USER`     | `admin`                            |
| `INFLUXDB_PASSWORD` | Strong password                    |
| `INFLUXDB_ORG`      | `heatpump`                         |
| `INFLUXDB_BUCKET`   | `sensor_data`                      |
| `INFLUXDB_RETENTION`| `30d`                              |
| `INFLUXDB_TOKEN`    | Random (`openssl rand -hex 32`)    |
| `MQTT_USER`         | `heatpump`                         |
| `MQTT_PASSWORD`     | Strong password                    |

Set these BEFORE the first deploy. InfluxDB init only runs once with empty volumes.

---

## How Deployments Work

1. Push to `main` with changes in `server/`
2. GitHub Actions builds **3 images** on GitHub runners, pushes to GHCR
3. SSHs into EC2, writes compose file + .env, runs `docker compose pull && up -d`
4. InfluxDB data persists in named Docker volumes
5. MQTT passwd is regenerated from env vars on every container start

**EC2 server contains:** `~/heatpump/docker-compose.yml` + `~/heatpump/.env`. Nothing else.

**Rollback:** SSH in, edit compose file to pin image tags to a previous commit SHA, `docker compose pull && up -d`.

---

## What stays the same for local dev

The existing `docker-compose.yml` with bind mounts continues to work for local development. The new Dockerfiles are additive — they don't break the current `./setup.sh` workflow.

---

## Verification

1. Push to `main`, check **Actions** tab for successful build + deploy
2. `http://<elastic-ip>/api/health` — InfluxDB and MQTT connected
3. `http://<elastic-ip>/` — dashboard loads
4. `python server/scripts/simulate_device.py --device-id site1 --broker <elastic-ip>` — data appears on dashboard
5. Flash ESP32 with updated `MQTT_BROKER` and verify real data flows
