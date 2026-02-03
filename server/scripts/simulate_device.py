#!/usr/bin/env python3
"""
Heat Pump Device Simulator
==========================

Simulates an ESP32 heat pump monitor sending data via MQTT.
Useful for testing the server without actual hardware.

Usage:
    python simulate_device.py [--device-id DEVICE_ID] [--broker BROKER] [--interval SECONDS]
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
    def __init__(self, device_id: str, broker: str, port: int, user: str, password: str):
        self.device_id = device_id
        self.broker = broker
        self.port = port
        self.user = user
        self.password = password

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
            # Publish online status
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

    def _generate_reading(self) -> dict:
        """Generate simulated sensor reading with realistic variations"""
        variation = random.uniform(-1.0, 1.0)

        # Simulate occasional anomalies (1% chance)
        anomaly = random.random() < 0.01

        data = {
            "device": self.device_id,
            "timestamp": int(time.time() * 1000),
            "temperature": {
                "inlet": round(self.base_temp_inlet + variation, 1),
                "outlet": round(self.base_temp_outlet + variation, 1),
                "ambient": round(self.base_temp_ambient + variation * 0.5, 1),
                "compressor": round(self.base_temp_compressor + variation * 2, 1)
            },
            "electrical": {
                "voltage": round(self.base_voltage + variation * 5, 1),
                "current": round(self.base_current + variation * 0.5, 2),
                "power": 0  # Calculated below
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

        # Calculate power
        data["electrical"]["power"] = round(
            data["electrical"]["voltage"] * data["electrical"]["current"], 0
        )

        # Simulate anomaly
        if anomaly:
            anomaly_type = random.choice(["voltage", "temp", "pressure"])
            if anomaly_type == "voltage":
                # High or low voltage
                if random.random() < 0.5:
                    data["electrical"]["voltage"] = round(random.uniform(250, 260), 1)
                    data["alerts"]["voltage"] = 2  # Critical
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

    def publish_reading(self):
        """Publish a sensor reading to MQTT"""
        data = self._generate_reading()
        topic = f"heatpump/{self.device_id}/data"
        payload = json.dumps(data)

        result = self.client.publish(topic, payload)

        self.reading_count += 1

        # Print summary
        print(f"\n[{datetime.now().strftime('%H:%M:%S')}] Reading #{self.reading_count}")
        print(f"  Temps: In={data['temperature']['inlet']}C Out={data['temperature']['outlet']}C "
              f"Amb={data['temperature']['ambient']}C Comp={data['temperature']['compressor']}C")
        print(f"  Elec:  {data['electrical']['voltage']}V {data['electrical']['current']}A "
              f"{data['electrical']['power']}W")
        print(f"  Press: Hi={data['pressure']['high']} Lo={data['pressure']['low']} PSI")

        # Check for alerts
        alerts = data['alerts']
        if any(v > 0 for v in alerts.values()):
            active = [k for k, v in alerts.items() if v > 0]
            print(f"  ALERTS: {', '.join(active)}")

        return result

    def run(self, interval: int):
        """Run the simulator continuously"""
        print(f"\nStarting Heat Pump Simulator")
        print(f"  Device ID: {self.device_id}")
        print(f"  Broker:    {self.broker}:{self.port}")
        print(f"  Interval:  {interval} seconds")
        print(f"\nPress Ctrl+C to stop\n")

        self.connect()
        time.sleep(2)  # Wait for connection

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

    args = parser.parse_args()

    simulator = HeatPumpSimulator(
        device_id=args.device_id,
        broker=args.broker,
        port=args.port,
        user=args.user,
        password=args.password
    )

    simulator.run(args.interval)


if __name__ == "__main__":
    main()
