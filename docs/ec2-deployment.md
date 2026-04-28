# Deploy to AWS EC2

## 1. EC2 Instance Setup (One-Time)

**Instance:** `t3.small` (~$15/mo)
**AMI:** Ubuntu 24.04 LTS | **Storage:** 20 GB gp3
**Elastic IP:** Allocate one (free while instance running)

**Security Group:**

| Port | Source      | Purpose                          |
|------|-------------|----------------------------------|
| 22   | Your IP     | SSH                              |
| 80   | 0.0.0.0/0   | HTTP (auto-redirects to HTTPS)   |
| 443  | 0.0.0.0/0   | HTTPS (Traefik + Let's Encrypt)  |
| 1883 | 0.0.0.0/0   | MQTT (ESP32 devices)             |

**SSH in and run:**
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

---

## 2. Domain DNS Records

Add these **A records** in your domain registrar's DNS dashboard, both pointing to the Elastic IP:

| Type | Name                          | Value             | Purpose          |
|------|-------------------------------|-------------------|------------------|
| A    | `hp.iot.aquaproducts.in`              | `<elastic-ip>` | Dashboard   |
| A    | `hp-data-explorer.iot.aquaproducts.in` | `<elastic-ip>` | InfluxDB UI |

Traefik automatically provisions Let's Encrypt SSL certificates once DNS propagates. No manual cert setup needed.

---

## 3. GitHub Secrets

Go to repo **Settings > Secrets and variables > Actions** and add all of these before the first deploy:

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
| `DOMAIN`            | `hp.iot.aquaproducts.in`              |
| `INFLUXDB_DOMAIN`   | `hp-data-explorer.iot.aquaproducts.in` |
| `ACME_EMAIL`        | Your email (for Let's Encrypt)     |

InfluxDB init only runs once with empty volumes — set credentials right the first time.

---

## 4. Deploy

Push to `main` with changes in `server/`, or trigger manually from the **Actions** tab.

**What happens:**
1. GitHub Actions builds 3 Docker images (backend, dashboard, mqtt) and pushes to GHCR
2. SCPs `server/docker-compose.prod.yml` to `~/heatpump` on EC2
3. SSHs into EC2, writes `.env` from GitHub Secrets, runs `docker compose -f docker-compose.prod.yml pull && up -d`

**EC2 server contains:** `~/heatpump/docker-compose.prod.yml` + `~/heatpump/.env`. Nothing else.

---

## 5. Verify

1. Check **Actions** tab for a green build + deploy
2. `https://<DOMAIN>/api/health` — should report InfluxDB and MQTT connected
3. `https://<DOMAIN>/` — dashboard loads
4. `https://<INFLUXDB_DOMAIN>/` — InfluxDB UI loads
5. Test MQTT: `mosquitto_pub -h <elastic-ip> -p 1883 -u <MQTT_USER> -P <MQTT_PASSWORD> -t "test" -m "ping"`
6. Flash ESP32 with `MQTT_BROKER` set to the Elastic IP

---

## 6. Rollback

SSH in, pin image tags to a previous commit SHA, and redeploy:
```bash
cd ~/heatpump
# Edit docker-compose.prod.yml: change :latest to :<commit-sha>
docker compose -f docker-compose.prod.yml pull
docker compose -f docker-compose.prod.yml up -d
```
