# Sensor Calibration Guide

This guide covers calibration procedures for all analog sensors in the smart heat pump monitoring system. All calibration constants live in `firmware/config.h`.

---

## Thermistors (NTC 10K)

Four 10K NTC thermistors are used in a voltage divider configuration with 10K series resistors. Temperature is computed using the Steinhart-Hart (B-parameter) equation in `firmware/src/sensors.cpp:149-179`.

### Configuration (`config.h:126-130`)

| Parameter | Default | Description |
|-----------|---------|-------------|
| `NTC_BETA` | 3950.0 | B-coefficient (from datasheet) |
| `NTC_NOMINAL_RESISTANCE` | 10000.0 | Resistance at 25°C |
| `NTC_NOMINAL_TEMP` | 25.0 | Reference temperature (°C) |
| `NTC_SERIES_RESISTANCE` | 10000.0 | Series resistor value |

### Sensor Channels

| Sensor | Pin | Purpose | Typical Range |
|--------|-----|---------|---------------|
| Inlet | GPIO 34 | Water/refrigerant inlet | 5–55°C |
| Outlet | GPIO 35 | Water/refrigerant outlet | 25–65°C |
| Ambient | GPIO 32 | Ambient air | -10–45°C |
| Compressor | GPIO 33 | Compressor body | 30–90°C |

### Calibration Steps

#### 1. Measure your series resistor

Use a multimeter to measure the actual resistance of each 10K series resistor. A "10K" resistor could be 9.85K or 10.12K. Update `NTC_SERIES_RESISTANCE` with the measured value.

#### 2. Ice-water reference point (0°C)

- Fill a cup with crushed ice, add water, stir for 2 minutes
- Submerge the thermistor probe
- Read the serial output — it should show ~0°C
- If it's off by more than 1°C, your Beta or nominal resistance needs adjustment

#### 3. Boiling-water reference point (100°C)

- Boil water (adjust for altitude: ~100°C at sea level)
- Hold the probe in the steam or water
- Compare reading to expected value

#### 4. Adjust `NTC_BETA` if needed

If both reference points are off, compute a more accurate Beta from two known temperature/resistance measurements:

```
B = ln(R1/R2) / (1/T1 - 1/T2)
```

Where T1 and T2 are in Kelvin, and R1 and R2 are measured resistances at those temperatures.

#### 5. Quick single-point calibration

If you have a reference thermometer:

- Place both sensors at the same location
- Read the raw ADC value from serial
- Back-calculate the resistance: `R = series_R * V / (3.3 - V)` where `V = ADC * 3.3 / 4095`
- If the computed temperature doesn't match your reference, adjust `NTC_NOMINAL_RESISTANCE` slightly

### Validation

Readings outside -40°C to 125°C (`TEMP_MIN_VALID` / `TEMP_MAX_VALID` in `config.h:179-180`) are flagged invalid. A `NAN` reading means open or shorted wiring (ADC reads 0 or 4095).

---

## Voltage Sensor (ZMPT101B)

The ZMPT101B is a transformer-coupled AC voltage sensor that outputs a sine wave centered at ~1.65V (half of 3.3V). The firmware samples the waveform, computes RMS, and scales it using a calibration factor. See `firmware/src/sensors.cpp:181-201`.

### Configuration (`config.h:132-134`)

| Parameter | Default | Description |
|-----------|---------|-------------|
| `VOLTAGE_SAMPLES` | 500 | Number of samples per RMS reading |
| `VOLTAGE_SCALE_FACTOR` | 234.26 | Calibrate with known voltage source |

**Pin:** GPIO 39 (ADC1_CH3, input only)

### Calibration Steps

#### 1. Get a reference voltage reading

Use a multimeter set to AC voltage on the same outlet/line you're measuring.

#### 2. Read the raw RMS from serial

Add a serial print of the raw RMS value (before scaling) or use the existing output.

#### 3. Compute the correct scale factor

```
VOLTAGE_SCALE_FACTOR = actual_voltage_from_multimeter / raw_rms_value
```

For example, if your multimeter reads 122V and the raw RMS computes to 0.521:

```
scale_factor = 122.0 / 0.521 = 234.16
```

#### 4. Update `config.h` and re-flash

#### 5. Verify across range (optional)

If you have a variac (variable transformer), test at multiple voltages (e.g., 80V, 120V, 240V) to confirm linearity.

### Tips

- The 234.26 default is a starting point — every ZMPT101B module varies slightly
- Ensure sampling captures at least one full AC cycle (500 samples at default ADC speed is usually enough for 50/60Hz)
- If readings drift, check that `ADC_CENTER_VALUE` (2048) matches your actual zero-crossing midpoint

---

## Current Sensor (ACS712-20A)

The ACS712-20A is a Hall-effect current sensor that outputs a voltage centered at VCC/2 (zero current) and swings ±100mV per Amp. See `firmware/src/sensors.cpp` for the RMS calculation.

### Configuration (`config.h:136-139`)

| Parameter | Default | Description |
|-----------|---------|-------------|
| `ACS712_SENSITIVITY` | 0.100 | 100mV per Amp (20A version) |
| `ACS712_ZERO_POINT` | 1.65 | Voltage at 0A (~VCC/2) |
| `CURRENT_SAMPLES` | 500 | Number of samples per RMS reading |

**Pin:** GPIO 36 (ADC1_CH0, input only)

### Calibration Steps

#### 1. Calibrate the zero-point (no load)

- Disconnect the load so no current flows through the sensor
- Read the raw ADC average and convert to voltage: `V = ADC * 3.3 / 4095`
- Update `ACS712_ZERO_POINT` with the measured value
- It's often not exactly 1.65V — could be 1.62V or 1.68V

#### 2. Calibrate sensitivity with a known load

- Use a clamp meter on the same wire as a reference
- Turn on a known, steady load (e.g., a resistive heater or incandescent bulb)
- Read the current from your clamp meter and from the sensor
- Compute corrected sensitivity:

```
ACS712_SENSITIVITY = measured_voltage_swing_rms / actual_current_from_clamp_meter
```

#### 3. Verify at multiple loads

Test at 2-3 different current levels to confirm the sensor is linear in your operating range.

### Troubleshooting

| Problem | Cause | Fix |
|---------|-------|-----|
| Non-zero reading with no load | Zero-point offset wrong | Recalibrate `ACS712_ZERO_POINT` |
| Reading ~50% too high/low | Wrong ACS712 variant | Check sensitivity: 185mV/A (5A), 100mV/A (20A), 66mV/A (30A) |
| Noisy readings | ADC noise / long wires | Add a 0.1µF cap at the sensor output, keep wires short |
| Reads 0A always | Wrong pin or ADC2 conflict | Confirm pin is on ADC1 (GPIO 36) |

---

## Pressure Transducer (0-500 PSI)

The pressure transducer outputs a linear 0.5–4.5V signal proportional to 0–500 PSI. See `firmware/src/sensors.cpp:227-247`.

### Configuration (`config.h:141-144`)

| Parameter | Default | Description |
|-----------|---------|-------------|
| `PRESSURE_MIN_VOLTAGE` | 0.5 | Output voltage at 0 PSI |
| `PRESSURE_MAX_VOLTAGE` | 4.5 | Output voltage at 500 PSI |
| `PRESSURE_MAX_PSI` | 500.0 | Full-scale pressure range |

### Calibration Steps

#### 1. Zero-pressure check

- With no pressure applied, the sensor should output ~0.5V
- If the reading isn't 0 PSI, adjust `PRESSURE_MIN_VOLTAGE`

#### 2. Known-pressure reference

- Use a calibrated pressure gauge on the same line
- Compare readings and adjust `PRESSURE_MAX_VOLTAGE` or `PRESSURE_MAX_PSI` if needed

### Notes

- The firmware clamps voltage to the 0.5–4.5V range before computing pressure
- Readings below 0.5V or above 4.5V indicate a wiring fault or sensor failure
- The ESP32 ADC maxes out at 3.3V — if your transducer outputs up to 4.5V, you need a voltage divider on the signal line to avoid clipping

---

## General ADC Notes (ESP32)

| Parameter | Value | Source |
|-----------|-------|--------|
| Resolution | 12-bit (0–4095) | `config.h:147` |
| Reference voltage | 3.3V | `config.h:149` |
| Attenuation | 11dB (full 0–3.3V range) | `sensors.cpp:17` |

- All sensor pins use **ADC1** channels, which is required since ADC2 conflicts with WiFi on ESP32
- The ADC on ESP32 has known nonlinearity at the extremes (below ~0.1V and above ~3.1V). For best accuracy, keep sensor outputs in the 0.2–3.0V range
