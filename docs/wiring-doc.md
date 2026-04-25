# Complete Wiring Guide: Smart Heat Pump Monitor

## All Components

| Component | Quantity | Purpose | Approx Price (₹) |
|-----------|----------|---------|------------------|
| ESP32 WROOM Dev Board | 1 | Main controller | 500 |
| SIM800C Development Board | 1 | GSM/SMS communication | 450 |
| 10K NTC Thermistor | 4 | Temperature sensing | 40 each |
| 10K Resistor (1/4W) | 4 | Thermistor voltage dividers | 5 each |
| ZMPT101B Module | 1 | AC Voltage sensing | 250 |
| SCT-013-020 CT Clamp | 1 | Current sensing (non-invasive) | 250 |
| Breadboard (830 points) | 1 | Prototyping | 150 |
| Jumper Wires (M-M) | 30+ | Connections | 100 |
| Micro SIM Card | 1 | Airtel/Vi with SMS pack | 50 |
| USB Cable | 1 | Power + Programming | 100 |
| 5V 2A Power Adapter | 1 | Reliable power (optional) | 200 |
| **Total** | | | **~₹2,200** |

### Optional (for full system)
| Component | Quantity | Purpose | Approx Price (₹) |
|-----------|----------|---------|------------------|
| Pressure Transducer (0-500 PSI) | 2 | Refrigerant pressure | 1,500 each |
| 5mm LED (Red) | 1 | Alarm indicator | 5 |
| 5mm LED (Green) | 1 | Status indicator | 5 |
| 330Ω Resistor | 2 | LED current limiting | 5 each |
| Buzzer (5V) | 1 | Audio alarm | 30 |

---

## Master Pin Assignment

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                         ESP32 PIN ASSIGNMENTS                                │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                              │
│   SENSOR INPUTS (Analog)                                                     │
│   ──────────────────────                                                     │
│   GPIO34 (ADC1_CH6) ◄─── Temperature: Inlet                                 │
│   GPIO35 (ADC1_CH7) ◄─── Temperature: Outlet                                │
│   GPIO32 (ADC1_CH4) ◄─── Temperature: Ambient                               │
│   GPIO33 (ADC1_CH5) ◄─── Temperature: Compressor                            │
│   GPIO36 (VP)       ◄─── Voltage Sensor (ZMPT101B)                          │
│   GPIO39 (VN)       ◄─── Current Sensor (SCT-013 CT Clamp)                  │
│   GPIO25           ◄─── Pressure: High Side (optional)                      │
│   GPIO26           ◄─── Pressure: Low Side (optional)                       │
│                                                                              │
│   SIM800C (UART)                                                             │
│   ──────────────                                                             │
│   GPIO16 (RX2)     ◄─── SIM800C TXD                                         │
│   GPIO17 (TX2)     ───► SIM800C RXD                                         │
│                                                                              │
│   STATUS OUTPUTS (Digital)                                                   │
│   ────────────────────────                                                   │
│   GPIO22           ───► Alarm LED (Red)                                      │
│   GPIO23           ───► Status LED (Green)                                   │
│   GPIO21           ───► Buzzer (optional)                                    │
│                                                                              │
│   POWER                                                                      │
│   ─────                                                                      │
│   VIN (5V)         ───► SIM800C 5V                                          │
│   3.3V             ───► Sensor power (thermistors, etc.)                    │
│   GND              ───► Common ground (all components)                       │
│                                                                              │
└─────────────────────────────────────────────────────────────────────────────┘
```

---

## ESP32 Pinout Reference

```
                         ┌───────────────────────┐
                         │         USB           │
                         │       ┌─────┐         │
                   EN ───┤1      │     │      30├─── GND
   Voltage ► GPIO36/VP ───┤2      │ESP32│      29├─── GPIO23 ──► Status LED
  CT Clamp ► GPIO39/VN ───┤3      │     │      28├─── GPIO22 ──► Alarm LED
   Temp 1  ► GPIO34 ───┤4      │WROOM│      27├─── GPIO1/TX0 (USB Serial)
   Temp 2  ► GPIO35 ───┤5      │     │      26├─── GPIO3/RX0 (USB Serial)
   Temp 3  ► GPIO32 ───┤6      │     │      25├─── GPIO21 ──► Buzzer
   Temp 4  ► GPIO33 ───┤7      └─────┘      24├─── GND
   Press H ► GPIO25 ───┤8                   23├─── GPIO19
   Press L ► GPIO26 ───┤9                   22├─── GPIO18
             GPIO27 ───┤10                  21├─── GPIO5
             GPIO14 ───┤11                  20├─── GPIO17 ──► SIM800C RXD
             GPIO12 ───┤12                  19├─── GPIO16 ◄── SIM800C TXD
               GND ───┤13                  18├─── GPIO4
               VIN ───┤14 ─────────────────────► SIM800C 5V
              3.3V ───┤15 ─────────────────────► Sensors VCC
                         └───────────────────────┘
```

---

## Section 1: SIM800C Wiring

### Components
- SIM800C Development Board
- 4 Jumper wires

### Connection Table

| ESP32 | SIM800C | Wire Color | Notes |
|-------|---------|------------|-------|
| VIN | 5V/VCC | Red | Power supply |
| GND | GND | Black | Common ground |
| GPIO16 | TXD | Yellow | ESP32 receives from SIM800C |
| GPIO17 | RXD | Green | ESP32 sends to SIM800C |

### Wiring Diagram

```
        ESP32                                      SIM800C Board
   ┌─────────────┐                              ┌─────────────────┐
   │             │                              │   [Antenna]     │
   │         VIN ├───────── Red ───────────────►│ 5V              │
   │             │                              │                 │
   │         GND ├───────── Black ─────────────►│ GND             │
   │             │                              │                 │
   │      GPIO16 │◄──────── Yellow ────────────│ TXD             │
   │             │                              │                 │
   │      GPIO17 ├───────── Green ────────────►│ RXD             │
   │             │                              │                 │
   │         USB │                              │ [SIM Card Slot] │
   └─────────────┘                              │    (on back)    │
                                                └─────────────────┘
```

### Before Powering On
- [ ] Insert Micro SIM card (Airtel/Vi)
- [ ] Disable PIN lock on SIM (use phone first)
- [ ] Attach antenna (required!)
- [ ] Verify TX↔RX are crossed

---

## Section 2: Temperature Sensors (NTC Thermistors)

### Components (per sensor)
- 10K NTC Thermistor
- 10K Resistor (for voltage divider)

### How It Works

```
Voltage Divider Circuit:

    3.3V
     │
     │
    ┌┴┐
    │ │ 10K Resistor (fixed)
    │ │
    └┬┘
     │
     ├────────────► To ESP32 ADC Pin (GPIO34/35/32/33)
     │
    ┌┴┐
    │ │ 10K NTC Thermistor (varies with temperature)
    │ │
    └┬┘
     │
    GND
```

As temperature increases → NTC resistance decreases → Voltage at ADC pin increases

### Connection Table (All 4 Sensors)

| Sensor | ESP32 Pin | Purpose |
|--------|-----------|---------|
| NTC 1 | GPIO34 | Inlet Temperature |
| NTC 2 | GPIO35 | Outlet Temperature |
| NTC 3 | GPIO32 | Ambient Temperature |
| NTC 4 | GPIO33 | Compressor Temperature |

### Breadboard Layout for ONE Temperature Sensor

```
                    Breadboard
    ┌─────────────────────────────────────────┐
    │                                         │
  + │ ● ─────────── 3.3V from ESP32           │ (Red power rail)
    │ │                                       │
    │ │   Column 10        Column 15          │
    │ │                                       │
  a │ ●────────────────────●                  │
    │ │                    │                  │
  b │ │   ┌──────────┐    │                  │
    │ │   │  10K     │    │                  │
  c │ └───┤ Resistor ├────●────► To GPIO34   │
    │     │  (brown) │    │                  │
  d │     └──────────┘    │                  │
    │                     │                  │
  e │         ┌───────────┘                  │
    │         │                              │
    │ ════════│══════════════════════════    │ (center gap)
    │         │                              │
  f │     ┌───┴───┐                          │
    │     │  NTC  │                          │
  g │     │Therm- │                          │
    │     │ istor │                          │
  h │     └───┬───┘                          │
    │         │                              │
  i │         ●                              │
    │         │                              │
  - │ ────────● ─────────── GND              │ (Blue ground rail)
    │                                         │
    └─────────────────────────────────────────┘
```

### All 4 Temperature Sensors on Breadboard

```
    3.3V Rail (+)
    ────●────────●────────●────────●────────
        │        │        │        │
       ┌┴┐      ┌┴┐      ┌┴┐      ┌┴┐
       │R│10K   │R│10K   │R│10K   │R│10K
       │ │      │ │      │ │      │ │
       └┬┘      └┬┘      └┬┘      └┬┘
        │        │        │        │
        ├──►34   ├──►35   ├──►32   ├──►33
        │        │        │        │
       ┌┴┐      ┌┴┐      ┌┴┐      ┌┴┐
       │N│NTC   │N│NTC   │N│NTC   │N│NTC
       │T│      │T│      │T│      │T│
       │C│      │C│      │C│      │C│
       └┬┘      └┬┘      └┬┘      └┬┘
        │        │        │        │
    ────●────────●────────●────────●────────
    GND Rail (-)

    Inlet    Outlet   Ambient  Compressor
```

---

## Section 3: Voltage Sensor (ZMPT101B)

### About the Module
- Measures AC voltage (0-250V)
- Outputs 0-3.3V analog signal
- Isolated (safe to use)

### Connection Table

| ZMPT101B | ESP32 | Notes |
|----------|-------|-------|
| VCC | 3.3V | Power (can also use 5V) |
| GND | GND | Common ground |
| OUT | GPIO36 (VP) | Analog output |

### Wiring Diagram

```
     AC Mains                    ZMPT101B Module                 ESP32
   (DANGEROUS!)                                               
                              ┌─────────────────┐
   ┌─────────┐               │                 │
   │ Live ───┼───────────────┤ AC IN      VCC ├──────────► 3.3V
   │         │               │                 │
   │ Neutral ┼───────────────┤ AC IN      GND ├──────────► GND
   └─────────┘               │                 │
                              │             OUT ├──────────► GPIO36 (VP)
      ⚠️ HIGH                 │                 │
      VOLTAGE!                └─────────────────┘
```

### ⚠️ SAFETY WARNING

```
╔═══════════════════════════════════════════════════════════════════╗
║                        ⚠️ DANGER ⚠️                                ║
║                                                                    ║
║   The AC input side of ZMPT101B connects to MAINS VOLTAGE          ║
║   (230V in India). This can KILL you!                              ║
║                                                                    ║
║   SAFETY RULES:                                                    ║
║   1. ALWAYS disconnect mains before touching wires                 ║
║   2. Use proper insulated wires for AC side                        ║
║   3. Keep AC wires away from low-voltage components                ║
║   4. Mount in proper enclosure for final installation              ║
║   5. If unsure, consult an electrician                             ║
║                                                                    ║
║   For PROTOTYPING: You can skip this sensor initially and          ║
║   use simulated values in code. Add real sensor later.             ║
║                                                                    ║
╚═══════════════════════════════════════════════════════════════════╝
```

---

## Section 4: Current Sensor (SCT-013-020 CT Clamp)

### About the Sensor
- Non-invasive AC current measurement (clamp-on)
- SCT-013-020: 20A max, built-in burden resistor, outputs 0-1V AC
- Fully isolated — no electrical contact with mains
- Output is AC signal centered at 0V, needs DC bias circuit for ESP32

### Components
- SCT-013-020 CT Clamp (with 3.5mm audio jack)
- 2x 10K Resistors (DC bias voltage divider)
- 1x 10µF Electrolytic Capacitor (bias smoothing)
- 3.5mm Audio Jack Breakout (or cut cable and solder)

### Connection Table

| CT Clamp Circuit | ESP32 | Notes |
|------------------|-------|-------|
| Signal (via bias circuit) | GPIO39 (VN) | Analog input |
| Bias divider | 3.3V + GND | Creates 1.65V midpoint |

### DC Bias Circuit (Required!)

The CT clamp outputs an AC signal that swings positive and negative.
ESP32 ADC only reads 0-3.3V, so we bias the signal to 1.65V midpoint:

```
    3.3V
     │
    ┌┴┐
    │ │ 10K Resistor
    │ │
    └┬┘
     │
     ├──────────────┬──────────────► GPIO39 (VN)
     │              │
     │          CT Clamp
     │          Signal Wire ─────┐
     │                           │
    ┌┴┐                     ┌────┴────┐
    │ │ 10K Resistor        │  10µF   │ (smoothing cap)
    │ │                     │  Cap    │
    └┬┘                     └────┬────┘
     │                           │
     ├───────────────────────────┘
     │
    GND
    (CT clamp other wire also to GND)

    Bias voltage = 3.3V × (10K / (10K + 10K)) = 1.65V
    CT signal rides on top of this 1.65V DC offset
```

### Wiring Diagram

```
     ┌────────────────────────┐
     │  SCT-013-020 CT Clamp  │
     │                        │
     │   ┌──────────────────┐ │
     │   │  Clamp around    │ │       DC Bias Circuit          ESP32
     │   │  ONE wire of     │ │
     │   │  the heat pump   │ │       3.3V
     │   │  power cable     │ │        │
     │   └──────────────────┘ │       ┌┴┐
     │                        │       │ │ 10K
     │    Signal ─────────────┼───────┤ │
     │    (red wire)          │       └┬┘
     │                        │        │
     │                        │        ├────┬────────────► GPIO39 (VN)
     │    GND ────────────────┼──┐     │    │
     │    (white wire)        │  │    ┌┴┐  ═══ 10µF
     │                        │  │    │ │ 10K  │
     └────────────────────────┘  │    └┬┘      │
                                 │     │       │
                                 └─────┴───────┴──────── GND
```

### Installation

```
    ⚡ Mains Cable to Heat Pump
    ════════════════════════════════════════

         ┌──────────────┐
         │   Separate   │    Clamp around ONE wire only
    ─────┤   the Live   ├──────────────────────────────
         │   wire       │
         └──────┬───────┘
                │
         ┌──────┴───────┐
         │  SCT-013-020 │
         │  CT Clamp    │
         └──────────────┘
                │
           Signal to ESP32

    ⚠️ IMPORTANT: Clamp around only ONE conductor (Live OR Neutral).
    If you clamp around the whole cable (Live + Neutral together),
    the magnetic fields cancel out and you read zero!
```

---

## Section 5: Status LEDs and Buzzer

### Components
- 1× Red LED (Alarm)
- 1× Green LED (Status)
- 2× 330Ω Resistors
- 1× Buzzer (optional)

### LED Wiring

```
                    330Ω
    GPIO22 ────────/\/\/\/────────┤>├──────── GND
                                 Red LED
                                (Alarm)

                    330Ω
    GPIO23 ────────/\/\/\/────────┤>├──────── GND
                                Green LED
                                (Status)
```

### LED on Breadboard

```
    GPIO22 ──────●
                 │
                ┌┴┐
                │ │ 330Ω Resistor
                └┬┘
                 │
                ┌┴┐
                │▼│ Red LED (long leg = anode here)
                └┬┘
                 │
    GND ─────────●
```

### Buzzer Wiring (Optional)

```
    GPIO21 ──────●
                 │
              ┌──┴──┐
              │     │
              │ BZR │  5V Buzzer
              │     │
              └──┬──┘
                 │
    GND ─────────●

    Note: If buzzer doesn't work directly, you may need 
    a transistor to drive it (ESP32 GPIO can only supply ~12mA)
```

### Buzzer with Transistor (for louder buzzers)

```
                                    5V (VIN)
                                      │
                                   ┌──┴──┐
                                   │ BZR │
                                   └──┬──┘
                                      │
                                      C
    GPIO21 ───── 1K Resistor ────── B  (2N2222 NPN Transistor)
                                      E
                                      │
                                     GND
```

---

## Section 6: Pressure Sensors (Optional)

### About Pressure Transducers
- Industrial sensors for refrigerant pressure
- Output: 0.5V - 4.5V (for 0-500 PSI)
- Require 5V power

### Connection Table

| Transducer Wire | ESP32 | Notes |
|-----------------|-------|-------|
| Red (VCC) | 5V (VIN) | Power |
| Black (GND) | GND | Ground |
| Yellow/Green (Signal) | GPIO25 or GPIO26 | Via voltage divider |

### Voltage Divider (same as ACS712)

Transducer outputs up to 4.5V, need to reduce for ESP32:

```
    Transducer Signal                ESP32
           │
          ┌┴┐
          │ │ 10K
          └┬┘
           │
           ├─────────────────────► GPIO25 (High Pressure)
           │                       GPIO26 (Low Pressure)
          ┌┴┐
          │ │ 20K
          └┬┘
           │
          GND
```

---

## Complete Breadboard Layout

```
═══════════════════════════════════════════════════════════════════════════════
                        COMPLETE SYSTEM - TOP VIEW
═══════════════════════════════════════════════════════════════════════════════

    Power Rails:
    ┌─────────────────────────────────────────────────────────────────────────┐
  + │ ●═══════════════════════════════════════════════════════════════════●  │ 3.3V
  - │ ●═══════════════════════════════════════════════════════════════════●  │ GND
    └─────────────────────────────────────────────────────────────────────────┘

    5V Rail (for SIM800C):
    ┌─────────────────────────────────────────────────────────────────────────┐
 5V │ ●═══════════════════════════════════════════════════════════════════●  │ From VIN
    └─────────────────────────────────────────────────────────────────────────┘

        Columns:  1    5    10   15   20   25   30   35   40   45   50   55   60

                 ┌──────────────────────────────────────────────────────────────┐
                 │                                                              │
              a  │  [E][S][P][3][2][ ][ ][ ][ ][ ][ ][ ][ ][ ][ ]             │
              b  │  [E][S][P][3][2][ ][ ][ ][ ][ ][ ][ ][ ][ ][ ]             │
              c  │  [ ][ ][ ][ ][ ][ ][ ][ ][ ][ ][ ][ ][ ][ ][ ]             │
              d  │  [ ][ ][ ][ ][ ][ ][ ][ ][ ][ ][ ][ ][ ][ ][ ]             │
              e  │  [ ][ ][ ][ ][ ][ ][ ][ ][ ][ ][ ][ ][ ][ ][ ]             │
                 │  ═══════════════════════════════════════════════            │
              f  │  [E][S][P][3][2][ ][ ][ ][ ][ ][ ][ ][ ][ ][ ]             │
              g  │  [E][S][P][3][2][ ][ ][ ][ ][ ][ ][ ][ ][ ][ ]             │
              h  │  [ ][ ][ ][ ][ ][ ][ ][ ][ ][ ][ ][ ][ ][ ][ ]             │
              i  │  [ ][ ][ ][ ][ ][ ][ ][ ][ ][ ][ ][ ][ ][ ][ ]             │
              j  │  [ ][ ][ ][ ][ ][ ][ ][ ][ ][ ][ ][ ][ ][ ][ ]             │
                 │                                                              │
                 │   │                    │                    │                │
                 │   ESP32               Sensors              Modules           │
                 │   (cols 1-15)         (cols 20-35)         (cols 40-60)      │
                 │                                                              │
                 └──────────────────────────────────────────────────────────────┘

    Components Placement:
    ─────────────────────
    • ESP32: Columns 1-15, straddling center gap
    • Temperature sensors: Columns 20-35
    • Voltage sensor + CT bias circuit: Columns 40-50
    • SIM800C: Off breadboard (use jumper wires)
    • LEDs: Columns 55-60
```

---

## Wiring Checklist

### Phase 1: Basic Setup (Test SIM800C First)
- [ ] ESP32 placed on breadboard
- [ ] Power rails connected (3.3V and GND from ESP32)
- [ ] 5V rail connected (VIN from ESP32)
- [ ] SIM800C wired (4 wires: 5V, GND, TX, RX)
- [ ] SIM card inserted, antenna attached
- [ ] **TEST: Upload SIM800C test code, verify "OK" response**

### Phase 2: Add Temperature Sensors
- [ ] First NTC + 10K resistor wired to GPIO34
- [ ] **TEST: Read analog value, verify it changes with temperature**
- [ ] Add remaining 3 NTC sensors (GPIO35, GPIO32, GPIO33)
- [ ] **TEST: All 4 sensors reading correctly**

### Phase 3: Add Voltage Sensor
- [ ] ZMPT101B connected (VCC, GND, OUT to GPIO36)
- [ ] **TEST: Read voltage value (CAREFUL with mains!)**
- [ ] Or skip and use simulated values for now

### Phase 4: Add Current Sensor (CT Clamp)
- [ ] DC bias circuit built (2x 10K divider + 10µF cap)
- [ ] Bias circuit output to GPIO39
- [ ] SCT-013-020 signal wire connected to bias circuit
- [ ] CT clamp clamped around ONE wire of heat pump power cable
- [ ] **TEST: Read current value (should be ~1.65V / ~0A with no load)**

### Phase 5: Add Status Indicators
- [ ] Red LED with resistor on GPIO22
- [ ] Green LED with resistor on GPIO23
- [ ] Buzzer on GPIO21 (optional)
- [ ] **TEST: Blink LEDs, beep buzzer**

### Phase 6: Full Integration
- [ ] All components connected
- [ ] Upload complete firmware
- [ ] **TEST: Full system test**

---

## Complete Wiring Summary Table

| Component | ESP32 Pin | Wire Color | Notes |
|-----------|-----------|------------|-------|
| **SIM800C** | | | |
| SIM800C 5V | VIN | Red | 5V Power |
| SIM800C GND | GND | Black | Ground |
| SIM800C TXD | GPIO16 | Yellow | Data from SIM800C |
| SIM800C RXD | GPIO17 | Green | Data to SIM800C |
| **Temperature Sensors** | | | |
| NTC 1 (Inlet) | GPIO34 | Orange | Via 10K divider |
| NTC 2 (Outlet) | GPIO35 | Orange | Via 10K divider |
| NTC 3 (Ambient) | GPIO32 | Orange | Via 10K divider |
| NTC 4 (Compressor) | GPIO33 | Orange | Via 10K divider |
| **Voltage Sensor** | | | |
| ZMPT101B VCC | 3.3V | Red | Power |
| ZMPT101B GND | GND | Black | Ground |
| ZMPT101B OUT | GPIO36 (VP) | Blue | Analog signal |
| **Current Sensor (CT Clamp)** | | | |
| CT Bias (3.3V) | 3.3V | Red | 10K resistor to bias node |
| CT Bias (GND) | GND | Black | 10K resistor to bias node |
| CT Signal | GPIO39 (VN) | Blue | Via DC bias circuit |
| **LEDs** | | | |
| Red LED | GPIO22 | Red | Via 330Ω resistor |
| Green LED | GPIO23 | Green | Via 330Ω resistor |
| **Buzzer** | | | |
| Buzzer + | GPIO21 | Any | Via transistor if needed |
| Buzzer - | GND | Black | Ground |
| **Power Rails** | | | |
| 3.3V Rail | 3.3V | Red | Sensor power |
| 5V Rail | VIN | Red | SIM800C |
| GND Rail | GND | Black | Common ground |

---

## Test Code: All Sensors

Once everything is wired, use this code to test all sensors:

```cpp
// Pin Definitions
#define SIM800_TX 17
#define SIM800_RX 16

#define TEMP_INLET 34
#define TEMP_OUTLET 35
#define TEMP_AMBIENT 32
#define TEMP_COMPRESSOR 33

#define VOLTAGE_PIN 36
#define CURRENT_PIN 39

#define ALARM_LED 22
#define STATUS_LED 23
#define BUZZER 21

void setup() {
  Serial.begin(115200);
  Serial2.begin(9600, SERIAL_8N1, SIM800_RX, SIM800_TX);
  
  // LED pins
  pinMode(ALARM_LED, OUTPUT);
  pinMode(STATUS_LED, OUTPUT);
  pinMode(BUZZER, OUTPUT);
  
  // ADC setup
  analogReadResolution(12);  // 12-bit (0-4095)
  
  delay(1000);
  Serial.println("\n\n========================================");
  Serial.println("Heat Pump Monitor - Sensor Test");
  Serial.println("========================================\n");
  
  // LED test
  Serial.println("Testing LEDs...");
  digitalWrite(STATUS_LED, HIGH);
  delay(500);
  digitalWrite(ALARM_LED, HIGH);
  delay(500);
  digitalWrite(STATUS_LED, LOW);
  digitalWrite(ALARM_LED, LOW);
  Serial.println("LEDs OK\n");
}

void loop() {
  // Blink status LED
  digitalWrite(STATUS_LED, HIGH);
  
  // Read all sensors
  Serial.println("--- Sensor Readings ---");
  
  // Temperature sensors (raw ADC values)
  int tempInlet = analogRead(TEMP_INLET);
  int tempOutlet = analogRead(TEMP_OUTLET);
  int tempAmbient = analogRead(TEMP_AMBIENT);
  int tempCompressor = analogRead(TEMP_COMPRESSOR);
  
  Serial.printf("Temp Inlet (raw):      %d\n", tempInlet);
  Serial.printf("Temp Outlet (raw):     %d\n", tempOutlet);
  Serial.printf("Temp Ambient (raw):    %d\n", tempAmbient);
  Serial.printf("Temp Compressor (raw): %d\n", tempCompressor);
  
  // Convert to temperature (approximate for 10K NTC)
  Serial.printf("Temp Inlet:      %.1f°C\n", rawToTemp(tempInlet));
  Serial.printf("Temp Outlet:     %.1f°C\n", rawToTemp(tempOutlet));
  Serial.printf("Temp Ambient:    %.1f°C\n", rawToTemp(tempAmbient));
  Serial.printf("Temp Compressor: %.1f°C\n", rawToTemp(tempCompressor));
  
  // Voltage sensor
  int voltageRaw = analogRead(VOLTAGE_PIN);
  Serial.printf("Voltage (raw):   %d\n", voltageRaw);
  
  // Current sensor
  int currentRaw = analogRead(CURRENT_PIN);
  Serial.printf("Current (raw):   %d\n", currentRaw);
  
  // SIM800C test
  Serial.println("\nSIM800C Response:");
  Serial2.println("AT");
  delay(500);
  while (Serial2.available()) {
    Serial.write(Serial2.read());
  }
  
  Serial.println("\n---------------------------\n");
  
  digitalWrite(STATUS_LED, LOW);
  delay(2000);
}

// Convert raw ADC to temperature (simplified for 10K NTC)
float rawToTemp(int raw) {
  if (raw == 0) return -999;  // Prevent divide by zero
  
  float voltage = raw * 3.3 / 4095.0;
  float resistance = 10000.0 * voltage / (3.3 - voltage);
  
  // Steinhart-Hart equation (simplified)
  float tempK = 1.0 / (1.0/298.15 + (1.0/3950.0) * log(resistance/10000.0));
  float tempC = tempK - 273.15;
  
  return tempC;
}
```

### Expected Output

```
========================================
Heat Pump Monitor - Sensor Test
========================================

Testing LEDs...
LEDs OK

--- Sensor Readings ---
Temp Inlet (raw):      2048
Temp Outlet (raw):     2100
Temp Ambient (raw):    2000
Temp Compressor (raw): 1950
Temp Inlet:      25.3°C
Temp Outlet:     27.1°C
Temp Ambient:    24.0°C
Temp Compressor: 22.8°C
Voltage (raw):   1850
Current (raw):   2048

SIM800C Response:
AT
OK

---------------------------
```

---

## Troubleshooting

| Problem | Possible Cause | Solution |
|---------|----------------|----------|
| Temperature reads -999 | No sensor connected or wrong pin | Check wiring, verify pin number |
| Temperature reads very high/low | Wrong resistor value | Use 10K resistor |
| All temps same value | Sensors shorted together | Check for bridged wires |
| Voltage always 0 | ZMPT101B not powered | Check VCC connection |
| Current always ~2048 | No current flowing (normal at 0A) | This is correct! 2048 = 0A (1.65V bias) |
| SIM800C no response | TX/RX swapped | Swap yellow and green wires |
| LEDs don't light | Wrong polarity or no resistor | Check LED direction, add resistor |

---

## Safety Reminders

```
╔═══════════════════════════════════════════════════════════════════╗
║                     ⚠️ SAFETY CHECKLIST ⚠️                         ║
╠═══════════════════════════════════════════════════════════════════╣
║                                                                    ║
║  □ Never touch AC wiring while powered                            ║
║  □ Keep AC and DC sides well separated                            ║
║  □ Use proper enclosure for final installation                    ║
║  □ Double-check all connections before powering on                ║
║  □ Start with low-voltage components first (skip ZMPT101B)        ║
║  □ Have an electrician review high-voltage connections            ║
║                                                                    ║
╚═══════════════════════════════════════════════════════════════════╝
```

---

## Next Steps

1. ✅ Wire SIM800C and test SMS
2. ✅ Wire temperature sensors and verify readings
3. ✅ Wire current sensor (skip voltage sensor initially)
4. ✅ Wire LEDs for status indication
5. ➡️ Upload complete firmware
6. ➡️ Set up server (MQTT + Web Dashboard)

Good luck with your build! 🔧