# V2 Wiring Guide: Smart Heat Pump Monitor (3-Phase)

## Components

| Component | Qty | Purpose | Approx Price |
|-----------|-----|---------|-------------|
| ESP32 WROOM Dev Board (34-pin) | 1 | Main controller | 500 |
| PZEM-004T v3.0 | 3 | 3-phase power monitoring (Modbus RTU) | 500 each |
| DS18B20 Waterproof Probe | 2 | Inlet/outlet temperature (OneWire) | 150 each |
| 4.7kOhm Resistor (1/4W) | 1 | OneWire pull-up resistor | 5 |
| SIM800C Development Board | 1 | GSM/SMS communication | 450 |
| Jumper Wires (M-M) | 20+ | Connections | 100 |
| Micro SIM Card | 1 | Airtel/Vi with SMS pack | 50 |
| USB Cable | 1 | Power + Programming | 100 |
| **Total** | | | **~3,000** |

---

## Master Pin Assignment

```
+-----------------------------------------------------------------------+
|                    ESP32 V2 PIN ASSIGNMENTS                            |
+-----------------------------------------------------------------------+
|                                                                        |
|   DS18B20 TEMPERATURE (OneWire Bus)                                    |
|   ---------------------------------                                    |
|   GPIO4              <--- DS18B20 DQ (data) - both sensors on bus      |
|                           + 4.7kOhm pull-up to 3.3V                   |
|                                                                        |
|   PZEM-004T v3.0 (Modbus RTU on UART1)                                |
|   ------------------------------------                                 |
|   GPIO18 (TX1)       ---> PZEM RX (all 3 modules in parallel)         |
|   GPIO19 (RX1)       <--- PZEM TX (all 3 modules in parallel)         |
|                                                                        |
|   SIM800C (UART2) - same as V1                                        |
|   --------------------------------                                     |
|   GPIO16 (RX2)       <--- SIM800C TXD                                 |
|   GPIO17 (TX2)       ---> SIM800C RXD                                 |
|                                                                        |
|   STATUS                                                               |
|   ------                                                               |
|   GPIO2              ---> Status LED                                   |
|                                                                        |
|   POWER                                                                |
|   -----                                                                |
|   VIN (5V)           ---> SIM800C 5V, PZEM VCC (all 3)                |
|   3.3V               ---> DS18B20 VDD, OneWire pull-up                 |
|   GND                ---> Common ground (all components)               |
|                                                                        |
+-----------------------------------------------------------------------+
```

---

## Section 1: DS18B20 Temperature Sensors (OneWire Bus)

### About DS18B20
- Digital temperature sensor with built-in ADC
- 1-Wire protocol: multiple sensors on single data pin
- Waterproof probe version ideal for pipe mounting
- Range: -55C to +125C, accuracy +/-0.5C
- 12-bit resolution (0.0625C)

### Wiring Diagram

```
    3.3V
     |
     +------------------+
     |                  |
     |                 [4.7kOhm]        Pull-up resistor
     |                  |                (REQUIRED for OneWire)
     |                  |
     +------ VDD       DQ ---+--------- GPIO4 (ESP32)
     |       |          |     |
     |  +----+----+     |     |
     |  | DS18B20 |     |     |
     |  | #1      |     |     |
     |  | (Inlet) |     |     |
     |  +----+----+     |     |
     |       |          |     |
     |      GND         |     |
     |       |          |     |
     |       |          |     |
     +------ VDD       DQ ---+
     |       |          |
     |  +----+----+     |
     |  | DS18B20 |     |
     |  | #2      |     |
     |  | (Outlet)|     |
     |  +----+----+     |
     |       |          |
     |      GND         |
     |       |          |
    GND------+----------+

    Wire Colors (waterproof probe):
      Red   = VDD (3.3V)
      Yellow = DQ (Data)
      Black  = GND
```

### Connection Table

| DS18B20 Wire | ESP32 | Notes |
|-------------|-------|-------|
| Red (VDD) | 3.3V | Power (both sensors) |
| Black (GND) | GND | Ground (both sensors) |
| Yellow (DQ) | GPIO4 | Data bus (both sensors share this pin) |
| 4.7kOhm | 3.3V to GPIO4 | Pull-up resistor between VDD and DQ |

### Sensor Identification

At first boot, the firmware scans the OneWire bus and prints sensor addresses:

```
[SENSORS] DS18B20 devices found: 2
[SENSORS] Inlet addr:  28FF12345678ABCD
[SENSORS] Outlet addr: 28FF87654321EFAB
```

The first sensor found is assigned as **inlet**, the second as **outlet**.
To swap, physically swap the sensors on the pipes.

---

## Section 2: PZEM-004T v3.0 (3-Phase Power Monitoring)

### About PZEM-004T v3.0
- Measures: Voltage, Current, Power, Energy, Frequency, Power Factor
- Communication: Modbus RTU over UART (9600 baud)
- Voltage range: 80-260V AC
- Current range: 0-100A (via external CT clamp)
- Multiple modules share one serial bus using different Modbus addresses

### Modbus Address Assignment

Each PZEM must have a unique address. **Set addresses before wiring all 3 together.**

| Module | Modbus Address | Phase |
|--------|---------------|-------|
| PZEM #1 | 0x01 (default) | Phase 1 (R) |
| PZEM #2 | 0x02 | Phase 2 (Y) |
| PZEM #3 | 0x03 | Phase 3 (B) |

**To change PZEM address:**
1. Connect ONE PZEM module at a time to ESP32
2. Upload the address-setting sketch:

```cpp
#include <PZEM004Tv30.h>
HardwareSerial PzemSerial(1);

void setup() {
    Serial.begin(115200);
    PzemSerial.begin(9600, SERIAL_8N1, 19, 18);  // RX=19, TX=18
    PZEM004Tv30 pzem(PzemSerial);

    // Change from default 0x01 to desired address
    pzem.setAddress(0x02);  // or 0x03 for the third module
    Serial.println("Address set!");
}

void loop() {}
```

3. Repeat for each module with the correct address

### Wiring Diagram (Communication Bus)

All 3 PZEMs share the same UART bus:

```
                                    +------ PZEM #1 (0x01) ------+
                                    |       RX  TX  VCC  GND     |
                                    |                            |
    ESP32                           +------ PZEM #2 (0x02) ------+
    GPIO18 (TX) ----+---------------+       RX  TX  VCC  GND     |
                    |               |                            |
    GPIO19 (RX) ----+---+           +------ PZEM #3 (0x03) ------+
                        |                   RX  TX  VCC  GND
                        |
                  All TX lines          All RX lines
                  connected             connected
                  together              together

    5V (VIN) --------+----- PZEM #1 VCC
                     +----- PZEM #2 VCC
                     +----- PZEM #3 VCC

    GND -------------+----- PZEM #1 GND
                     +----- PZEM #2 GND
                     +----- PZEM #3 GND
```

### AC Side Wiring (Per Phase)

Each PZEM-004T has screw terminals for AC measurement:

```
    3-Phase Mains Supply                    Heat Pump
    ====================                    =========

    Phase 1 (R) ----+                    +---- Phase 1
                    |   +-----------+   |
                    +---| PZEM #1   |---+
                        | L_IN L_OUT|
                        |           |
    Neutral ------+-----| N_IN N_OUT|-----+----- Neutral
                  |     +-----------+     |
                  |                       |
    Phase 2 (Y) --+-+                  +--+-- Phase 2
                    |   +-----------+  |
                    +---| PZEM #2   |--+
                        | L_IN L_OUT|
                        |           |
    Neutral ------+-----| N_IN N_OUT|-----+----- Neutral
                  |     +-----------+     |
                  |                       |
    Phase 3 (B) --+-+                  +--+-- Phase 3
                    |   +-----------+  |
                    +---| PZEM #3   |--+
                        | L_IN L_OUT|
                        |           |
    Neutral ------+-----| N_IN N_OUT|-----+----- Neutral
                        +-----------+
```

### Safety Warning

```
+===================================================================+
|                        WARNING: HIGH VOLTAGE                       |
|                                                                    |
|   The PZEM-004T AC terminals connect to MAINS VOLTAGE             |
|   (230V in India). This can KILL you!                              |
|                                                                    |
|   1. ALWAYS disconnect mains before touching AC wiring             |
|   2. Use properly rated wires for AC connections                   |
|   3. Mount PZEMs in a proper enclosure                             |
|   4. Have an electrician verify high-voltage connections           |
|   5. The low-voltage (communication) side is isolated              |
|                                                                    |
+===================================================================+
```

---

## Section 3: SIM800C Wiring (Same as V1)

| ESP32 | SIM800C | Wire Color | Notes |
|-------|---------|------------|-------|
| VIN | 5V/VCC | Red | Power supply |
| GND | GND | Black | Common ground |
| GPIO16 | TXD | Yellow | ESP32 receives from SIM800C |
| GPIO17 | RXD | Green | ESP32 sends to SIM800C |

```
    ESP32                           SIM800C Board
    +-------------+                 +-----------------+
    |         VIN |--- Red -------->| 5V              |
    |         GND |--- Black ------>| GND             |
    |      GPIO16 |<-- Yellow ------| TXD             |
    |      GPIO17 |--- Green ------>| RXD             |
    +-------------+                 +-----------------+
```

---

## Complete Wiring Summary

| Component | ESP32 Pin | Wire | Notes |
|-----------|-----------|------|-------|
| **DS18B20 Temperature** | | | |
| DS18B20 DQ (both) | GPIO4 | Yellow | Shared OneWire bus |
| DS18B20 VDD (both) | 3.3V | Red | Power |
| DS18B20 GND (both) | GND | Black | Ground |
| 4.7kOhm pull-up | 3.3V to GPIO4 | -- | Required for OneWire |
| **PZEM-004T (x3)** | | | |
| PZEM RX (all 3) | GPIO18 | Blue | ESP32 TX -> PZEM RX |
| PZEM TX (all 3) | GPIO19 | White | PZEM TX -> ESP32 RX |
| PZEM VCC (all 3) | 5V (VIN) | Red | Power |
| PZEM GND (all 3) | GND | Black | Ground |
| **SIM800C** | | | |
| SIM800C 5V | VIN | Red | Power |
| SIM800C GND | GND | Black | Ground |
| SIM800C TXD | GPIO16 | Yellow | Data from SIM800C |
| SIM800C RXD | GPIO17 | Green | Data to SIM800C |
| **Status LED** | | | |
| LED (built-in) | GPIO2 | -- | On-board LED |

---

## Assembly Checklist

### Phase 1: DS18B20 Temperature Sensors
- [ ] Wire 4.7kOhm pull-up resistor between GPIO4 and 3.3V
- [ ] Connect DS18B20 #1 (inlet): Red to 3.3V, Black to GND, Yellow to GPIO4
- [ ] Connect DS18B20 #2 (outlet): Red to 3.3V, Black to GND, Yellow to GPIO4
- [ ] **TEST:** Upload OneWire scanner sketch, verify 2 addresses printed

### Phase 2: PZEM-004T Address Configuration
- [ ] Connect PZEM #1 alone to GPIO18/19
- [ ] Verify default address 0x01 works (read voltage)
- [ ] Connect PZEM #2 alone, set address to 0x02
- [ ] Connect PZEM #3 alone, set address to 0x03
- [ ] **TEST:** Connect all 3, read each by address

### Phase 3: PZEM Communication Bus
- [ ] Wire all 3 PZEM RX pins to GPIO18
- [ ] Wire all 3 PZEM TX pins to GPIO19
- [ ] Wire all 3 PZEM VCC to 5V, GND to GND
- [ ] **TEST:** Read voltage/current from all 3 phases

### Phase 4: SIM800C (same as V1)
- [ ] Wire SIM800C (4 wires: 5V, GND, TX, RX)
- [ ] Insert SIM card, attach antenna
- [ ] **TEST:** Send AT command, verify "OK"

### Phase 5: AC Wiring (ELECTRICIAN RECOMMENDED)
- [ ] Wire PZEM #1 AC terminals to Phase 1
- [ ] Wire PZEM #2 AC terminals to Phase 2
- [ ] Wire PZEM #3 AC terminals to Phase 3
- [ ] Connect all neutral wires
- [ ] **TEST:** Power on, verify voltage readings on all 3 phases

### Phase 6: Full Integration
- [ ] Upload firmware-v2
- [ ] Verify all sensors reading
- [ ] Verify MQTT publishing
- [ ] Verify dashboard shows 3 phases
- [ ] Mount DS18B20 probes on inlet/outlet pipes

---

## Troubleshooting

| Problem | Possible Cause | Solution |
|---------|----------------|----------|
| DS18B20 not found | Missing pull-up resistor | Add 4.7kOhm between GPIO4 and 3.3V |
| Only 1 DS18B20 found | Wiring issue on second sensor | Check data wire connection |
| DS18B20 reads -127C | Sensor disconnected | Check wiring, verify address |
| PZEM reads NaN | Wrong address or no AC power | Verify Modbus address, check AC wiring |
| All 3 PZEMs same reading | Same address on all modules | Set unique addresses (0x01, 0x02, 0x03) |
| PZEM voltage = 0 | AC not connected | Wire L_IN/L_OUT terminals |
| PZEM current = 0 | CT clamp not installed | Clamp CT around ONE wire per phase |
| SIM800C no response | TX/RX swapped | Swap yellow and green wires |
