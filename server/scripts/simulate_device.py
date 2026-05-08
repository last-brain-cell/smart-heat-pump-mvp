#!/usr/bin/env python3
"""
Heat Pump Device Simulator
==========================

Simulates an ESP32 heat pump monitor sending data via MQTT.
Supports both v1 (single-phase) and v2 (3-phase PZEM) payloads.

Usage:
    python simulate_device.py [--version v1|v2] [--device-id DEVICE_ID] [--broker BROKER]
"""

import json
import time
import random
import argparse
from datetime import datetime

import paho.mqtt.client as mqtt

# Default configuration
DEFAULT_BROKER = "localhost"
DEFAULT_PORT = 1883
DEFAULT_USER = "heatpump"
DEFAULT_PASSWORD = "heatpump123"
DEFAULT_DEVICE_ID = "site1"
DEFAULT_INTERVAL = 10  # seconds

class HeatPumpSimulator:
    def __init__(self, device_id: str, broker: str, port: int, user: str, password: str, version: str = "v1"):
        self.device_id = device_id
        self.broker = broker
        self.port = port
        self.user = user
        self.password = password
        self.version = version

        # Base values for simulation
        self.base_temp_inlet = 45.0
        self.base_temp_outlet = 50.0
        self.base_temp_ambient = 25.0
        self.base_temp_compressor = 70.0
        self.base_voltage = 230.0
        self.base_current = 8.5
        self.base_pressure_high = 280.0
        self.base_pressure_low = 70.0

        # State
        self.compressor_running = True
        self.fan_running = True
        self.defrost_active = False
        self.reading_count = 0

        # MQTT client
        self.client = mqtt.Client()
        self.client.username_pw_set(user, password)
        self.client.on_connect = self._on_connect
        self.client.on_disconnect = self._on_disconnect

    def _on_connect(self, client, userdata, flags, rc):
        if rc == 0:
            print(f"[MQTT] Connected to {self.broker}:{self.port}")
            self._publish_status(True)
        else:
            print(f"[MQTT] Connection failed with code {rc}")

    def _on_disconnect(self, client, userdata, rc):
        print(f"[MQTT] Disconnected (rc={rc})")

    def connect(self):
        print(f"[MQTT] Connecting to {self.broker}:{self.port}...")
        self.client.connect(self.broker, self.port, 60)
        self.client.loop_start()

    def disconnect(self):
        self._publish_status(False)
        self.client.loop_stop()
        self.client.disconnect()

    def _publish_status(self, online: bool):
        topic = f"heatpump/{self.device_id}/status/online"
        self.client.publish(topic, "true" if online else "false", retain=True)

    def _gen_phase(self, variation, phase_idx):
        """Generate simulated data for a single phase."""
        offset = phase_idx * 0.5
        v = round(self.base_voltage + variation * 5 + offset, 1)
        c = round(self.base_current + variation * 0.5 - offset * 0.2, 2)
        pf = round(max(0.80, min(1.0, 0.95 + variation * 0.02)), 2)
        return {
            "voltage": v,
            "current": c,
            "power": round(v * c * pf, 1),
            "energy": round(100 + phase_idx * 10 + random.uniform(0, 5), 2),
            "frequency": round(50.0 + variation * 0.1, 1),
            "power_factor": pf,
        }

    def _generate_v1_reading(self) -> dict:
        """Generate v1 (single-phase) simulated sensor reading."""
        variation = random.uniform(-1.0, 1.0)
        anomaly = random.random() < 0.01

        data = {
            "device": self.device_id,
            "timestamp": int(time.time() * 1000),
            "version": "1.0.0",
            "temperature": {
                "inlet": round(self.base_temp_inlet + variation, 1),
                "outlet": round(self.base_temp_outlet + variation, 1),
                "ambient": round(self.base_temp_ambient + variation * 0.5, 1),
                "compressor": round(self.base_temp_compressor + variation * 2, 1)
            },
            "electrical": {
                "voltage": round(self.base_voltage + variation * 5, 1),
                "current": round(self.base_current + variation * 0.5, 2),
                "power": 0
            },
            "pressure": {
                "high": round(self.base_pressure_high + variation * 10, 0),
                "low": round(self.base_pressure_low + variation * 5, 0)
            },
            "status": {
                "compressor": self.compressor_running,
                "fan": self.fan_running,
                "defrost": self.defrost_active
            },
            "alerts": {
                "voltage": 0,
                "compressor_temp": 0,
                "pressure_high": 0,
                "pressure_low": 0,
                "current": 0
            }
        }

        data["electrical"]["power"] = round(
            data["electrical"]["voltage"] * data["electrical"]["current"], 0
        )

        if anomaly:
            anomaly_type = random.choice(["voltage", "temp", "pressure"])
            if anomaly_type == "voltage":
                if random.random() < 0.5:
                    data["electrical"]["voltage"] = round(random.uniform(250, 260), 1)
                    data["alerts"]["voltage"] = 2
                else:
                    data["electrical"]["voltage"] = round(random.uniform(200, 210), 1)
                    data["alerts"]["voltage"] = 2
            elif anomaly_type == "temp":
                data["temperature"]["compressor"] = round(random.uniform(90, 100), 1)
                data["alerts"]["compressor_temp"] = 2
            elif anomaly_type == "pressure":
                data["pressure"]["high"] = round(random.uniform(450, 480), 0)
                data["alerts"]["pressure_high"] = 2

        return data

    def _generate_v2_reading(self) -> dict:
        """Generate v2 (3-phase PZEM) simulated sensor reading."""
        variation = random.uniform(-1.0, 1.0)

        data = {
            "device": self.device_id,
            "timestamp": int(time.time() * 1000),
            "version": "2.0.0",
            "temperature": {
                "inlet": round(self.base_temp_inlet + variation, 1),
                "outlet": round(self.base_temp_outlet + variation, 1),
            },
            "electrical": {
                "phase1": self._gen_phase(variation, 0),
                "phase2": self._gen_phase(variation, 1),
                "phase3": self._gen_phase(variation, 2),
            },
            "status": {
                "compressor": self.compressor_running,
                "fan": self.fan_running,
                "defrost": self.defrost_active,
            },
            "alerts": {
                "voltage_p1": 0, "voltage_p2": 0, "voltage_p3": 0,
                "current_p1": 0, "current_p2": 0, "current_p3": 0,
            },
        }
        return data

    def _generate_reading(self) -> dict:
        if self.version == "v2":
            return self._generate_v2_reading()
        return self._generate_v1_reading()

    def publish_reading(self):
        """Publish a sensor reading to MQTT"""
        data = self._generate_reading()
        topic = f"heatpump/{self.device_id}/data"
        payload = json.dumps(data)

        result = self.client.publish(topic, payload)

        self.reading_count += 1

        ts = datetime.now().strftime('%H:%M:%S')

        if self.version == "v2":
            e = data["electrical"]
            print(f"\n[{ts}] V2 Reading #{self.reading_count}")
            print(f"  Temps: In={data['temperature']['inlet']}C Out={data['temperature']['outlet']}C")
            print(f"  P1: {e['phase1']['voltage']}V {e['phase1']['current']}A {e['phase1']['power']}W PF={e['phase1']['power_factor']}")
            print(f"  P2: {e['phase2']['voltage']}V {e['phase2']['current']}A {e['phase2']['power']}W PF={e['phase2']['power_factor']}")
            print(f"  P3: {e['phase3']['voltage']}V {e['phase3']['current']}A {e['phase3']['power']}W PF={e['phase3']['power_factor']}")
        else:
            print(f"\n[{ts}] V1 Reading #{self.reading_count}")
            print(f"  Temps: In={data['temperature']['inlet']}C Out={data['temperature']['outlet']}C "
                  f"Amb={data['temperature']['ambient']}C Comp={data['temperature']['compressor']}C")
            print(f"  Elec:  {data['electrical']['voltage']}V {data['electrical']['current']}A "
                  f"{data['electrical']['power']}W")
            print(f"  Press: Hi={data['pressure']['high']} Lo={data['pressure']['low']} PSI")

            alerts = data['alerts']
            if any(v > 0 for v in alerts.values()):
                active = [k for k, v in alerts.items() if v > 0]
                print(f"  ALERTS: {', '.join(active)}")

        return result

    def run(self, interval: int):
        """Run the simulator continuously"""
        print(f"\nStarting Heat Pump Simulator")
        print(f"  Device ID: {self.device_id}")
        print(f"  Version:   {self.version}")
        print(f"  Broker:    {self.broker}:{self.port}")
        print(f"  Interval:  {interval} seconds")
        print(f"\nPress Ctrl+C to stop\n")

        self.connect()
        time.sleep(2)

        try:
            while True:
                self.publish_reading()
                time.sleep(interval)
        except KeyboardInterrupt:
            print("\n\nStopping simulator...")
        finally:
            self.disconnect()
            print("Simulator stopped")


def main():
    parser = argparse.ArgumentParser(description="Heat Pump Device Simulator")
    parser.add_argument("--device-id", default=DEFAULT_DEVICE_ID,
                        help=f"Device ID (default: {DEFAULT_DEVICE_ID})")
    parser.add_argument("--broker", default=DEFAULT_BROKER,
                        help=f"MQTT broker address (default: {DEFAULT_BROKER})")
    parser.add_argument("--port", type=int, default=DEFAULT_PORT,
                        help=f"MQTT broker port (default: {DEFAULT_PORT})")
    parser.add_argument("--user", default=DEFAULT_USER,
                        help=f"MQTT username (default: {DEFAULT_USER})")
    parser.add_argument("--password", default=DEFAULT_PASSWORD,
                        help=f"MQTT password (default: {DEFAULT_PASSWORD})")
    parser.add_argument("--interval", type=int, default=DEFAULT_INTERVAL,
                        help=f"Publish interval in seconds (default: {DEFAULT_INTERVAL})")
    parser.add_argument("--version", choices=["v1", "v2"], default="v1",
                        help="Firmware version to simulate (default: v1)")

    args = parser.parse_args()

    simulator = HeatPumpSimulator(
        device_id=args.device_id,
        broker=args.broker,
        port=args.port,
        user=args.user,
        password=args.password,
        version=args.version,
    )

    simulator.run(args.interval)


if __name__ == "__main__":
    main()
