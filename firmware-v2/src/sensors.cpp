/**
 * @file sensors.cpp
 * @brief V2 sensor reading implementation
 *
 * DS18B20 (OneWire) for temperature + PZEM-004T v3.0 (Modbus) for 3-phase power.
 */

#include "sensors.h"
#include "globals.h"
#include <OneWire.h>
#include <DallasTemperature.h>
#include <PZEM004Tv30.h>

// =============================================================================
// DS18B20 SETUP
// =============================================================================

static OneWire oneWire(PIN_ONEWIRE);
static DallasTemperature tempSensors(&oneWire);
static DeviceAddress addrInlet, addrOutlet;
static bool tempSensorsFound = false;

// =============================================================================
// PZEM-004T SETUP — 3 modules on HardwareSerial1 (shared Modbus bus)
// =============================================================================

static HardwareSerial PzemSerial(1);  // UART1
static PZEM004Tv30* pzem[3] = {nullptr, nullptr, nullptr};

// =============================================================================
// IMPLEMENTATION
// =============================================================================

void initSensors() {
    // --- DS18B20 OneWire ---
    tempSensors.begin();
    tempSensors.setResolution(DS18B20_RESOLUTION);
    tempSensors.setWaitForConversion(false);  // Non-blocking reads

    int deviceCount = tempSensors.getDeviceCount();
    Log.print(F("[SENSORS] DS18B20 devices found: "));
    Log.println(deviceCount);

    if (deviceCount >= 2) {
        tempSensors.getAddress(addrInlet, 0);
        tempSensors.getAddress(addrOutlet, 1);
        tempSensorsFound = true;

        Log.print(F("[SENSORS] Inlet addr:  "));
        for (int i = 0; i < 8; i++) { Log.printf("%02X", addrInlet[i]); }
        Log.println();
        Log.print(F("[SENSORS] Outlet addr: "));
        for (int i = 0; i < 8; i++) { Log.printf("%02X", addrOutlet[i]); }
        Log.println();
    } else if (deviceCount == 1) {
        tempSensors.getAddress(addrInlet, 0);
        tempSensorsFound = true;
        Log.println(F("[SENSORS] WARNING: Only 1 DS18B20 found, using as inlet"));
    } else {
        Log.println(F("[SENSORS] WARNING: No DS18B20 sensors found!"));
    }

    // --- PZEM-004T on Serial1 ---
    // ESP32 HardwareSerial constructor requires (port, rxPin, txPin, addr)
    pzem[0] = new PZEM004Tv30(PzemSerial, PIN_PZEM_RX, PIN_PZEM_TX, PZEM_ADDR_PHASE1);
    pzem[1] = new PZEM004Tv30(PzemSerial, PIN_PZEM_RX, PIN_PZEM_TX, PZEM_ADDR_PHASE2);
    pzem[2] = new PZEM004Tv30(PzemSerial, PIN_PZEM_RX, PIN_PZEM_TX, PZEM_ADDR_PHASE3);

    Log.println(F("[SENSORS] PZEM-004T initialized (3 phases)"));

    // --- Status LED ---
    pinMode(PIN_STATUS_LED, OUTPUT);
    digitalWrite(PIN_STATUS_LED, LOW);

    Log.println(F("[SENSORS] V2 sensor init complete"));
}

bool isValidReading(float value, float minValid, float maxValid) {
    return !isnan(value) && value >= minValid && value <= maxValid;
}

/**
 * @brief Read one PZEM-004T module
 */
static void readPhase(int idx, PhaseData& phase, unsigned long timestamp) {
    if (!pzem[idx]) {
        phase.valid = false;
        return;
    }

    float v  = pzem[idx]->voltage();
    float c  = pzem[idx]->current();
    float p  = pzem[idx]->power();
    float e  = pzem[idx]->energy();
    float f  = pzem[idx]->frequency();
    float pf = pzem[idx]->pf();

    phase.voltage.value     = v;
    phase.voltage.timestamp = timestamp;
    phase.voltage.valid     = isValidReading(v, VOLTAGE_MIN_VALID, VOLTAGE_MAX_VALID);

    phase.current.value     = c;
    phase.current.timestamp = timestamp;
    phase.current.valid     = isValidReading(c, CURRENT_MIN_VALID, CURRENT_MAX_VALID);

    phase.power       = isnan(p)  ? 0.0f : p;
    phase.energy      = isnan(e)  ? 0.0f : e;
    phase.frequency   = isnan(f)  ? 0.0f : f;
    phase.powerFactor = isnan(pf) ? 0.0f : pf;

    // Phase is valid if we got voltage and current readings
    phase.valid = phase.voltage.valid && phase.current.valid;
}

SystemData readAllSensors() {
    if (SIMULATION_MODE) {
        return simulateSensors();
    }

    SystemData data;
    data.readingTime = millis();

    // --- DS18B20 temperatures ---
    tempSensors.requestTemperatures();
    // DS18B20 at 12-bit needs ~750ms; with non-blocking mode the previous
    // conversion result is returned. At 2s polling this is fine.

    if (tempSensorsFound) {
        float inlet = tempSensors.getTempC(addrInlet);
        data.tempInlet.value     = inlet;
        data.tempInlet.timestamp = data.readingTime;
        data.tempInlet.valid     = isValidReading(inlet, TEMP_MIN_VALID, TEMP_MAX_VALID);

        float outlet = tempSensors.getTempC(addrOutlet);
        data.tempOutlet.value     = outlet;
        data.tempOutlet.timestamp = data.readingTime;
        data.tempOutlet.valid     = isValidReading(outlet, TEMP_MIN_VALID, TEMP_MAX_VALID);
    }

    // --- 3-phase PZEM ---
    for (int i = 0; i < 3; i++) {
        readPhase(i, data.phase[i], data.readingTime);
    }

    // --- Compressor detection: total current > 1A ---
    float totalCurrent = 0;
    for (int i = 0; i < 3; i++) {
        if (data.phase[i].current.valid) {
            totalCurrent += data.phase[i].current.value;
        }
    }
    data.compressorRunning = totalCurrent > 1.0f;

    return data;
}

SystemData simulateSensors() {
    SystemData data;
    data.readingTime = millis();
    float v = (random(-100, 100) / 100.0f);

    // Temperatures
    data.tempInlet.value     = 45.0f + v;
    data.tempInlet.timestamp = data.readingTime;
    data.tempInlet.valid     = true;

    data.tempOutlet.value     = 50.0f + v;
    data.tempOutlet.timestamp = data.readingTime;
    data.tempOutlet.valid     = true;

    // 3-phase electrical
    for (int i = 0; i < 3; i++) {
        float phaseOffset = i * 0.5f;

        data.phase[i].voltage.value     = 230.0f + v * 5.0f + phaseOffset;
        data.phase[i].voltage.timestamp = data.readingTime;
        data.phase[i].voltage.valid     = true;

        data.phase[i].current.value     = 8.5f + v * 0.5f - phaseOffset * 0.2f;
        data.phase[i].current.timestamp = data.readingTime;
        data.phase[i].current.valid     = true;

        float pf = 0.95f + v * 0.02f;
        data.phase[i].power       = data.phase[i].voltage.value * data.phase[i].current.value * pf;
        data.phase[i].energy      = 100.0f + i * 10.0f + v;
        data.phase[i].frequency   = 50.0f + v * 0.1f;
        data.phase[i].powerFactor = pf;
        data.phase[i].valid       = true;
    }

    data.compressorRunning = true;
    data.fanRunning        = true;
    data.defrostActive     = false;

    return data;
}

void printSensorData(const SystemData& data) {
    Log.println(F("========== V2 SENSOR READINGS =========="));
    Log.print(F("Time: "));
    Log.println(data.readingTime);

    Log.println(F("--- Temperatures (DS18B20) ---"));
    Log.print(F("  Inlet:  "));
    Log.print(data.tempInlet.value, 1);
    Log.println(data.tempInlet.valid ? "" : " [INVALID]");
    Log.print(F("  Outlet: "));
    Log.print(data.tempOutlet.value, 1);
    Log.println(data.tempOutlet.valid ? "" : " [INVALID]");

    for (int i = 0; i < 3; i++) {
        Log.printf("\n--- Phase %d (PZEM) ---\n", i + 1);
        if (!data.phase[i].valid) {
            Log.println(F("  [NOT CONNECTED]"));
            continue;
        }
        Log.printf("  Voltage: %.1f V\n", data.phase[i].voltage.value);
        Log.printf("  Current: %.2f A\n", data.phase[i].current.value);
        Log.printf("  Power:   %.1f W\n", data.phase[i].power);
        Log.printf("  Energy:  %.2f kWh\n", data.phase[i].energy);
        Log.printf("  Freq:    %.1f Hz\n", data.phase[i].frequency);
        Log.printf("  PF:      %.2f\n", data.phase[i].powerFactor);
    }

    Log.print(F("\nCompressor: "));
    Log.println(data.compressorRunning ? "ON" : "OFF");
    Log.println(F("========================================="));
}
