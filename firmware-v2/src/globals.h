/**
 * @file globals.h
 * @brief Central declarations for shared global objects (V2)
 *
 * Identical to v1 globals.h except it uses v2 types.h
 * (SystemData contains PhaseData[3] instead of flat voltage/current/pressure).
 */

#ifndef GLOBALS_H
#define GLOBALS_H

#include <Arduino.h>

// TinyGSM configuration
#define TINY_GSM_MODEM_SIM800
#define TINY_GSM_RX_BUFFER 256

#include <WiFi.h>
#include <TinyGsmClient.h>
#include <PubSubClient.h>

#include "types.h"
#include "provision.h"
#include "log_capture.h"

// =============================================================================
// DEVICE IDENTITY (derived from ESP32 MAC at boot)
// =============================================================================

extern char deviceId[12];
extern char mqttTopicBase[32];

// =============================================================================
// GLOBAL OBJECT DECLARATIONS
// =============================================================================

extern TinyGsm modem;
extern TinyGsmClient gsmClient;
extern GSMState gsmState;
extern PubSubClient mqtt;
extern SystemData currentData;
extern bool networkReady;
extern bool startupComplete;
extern WiFiClient wifiClient;
extern ConnectionType activeConnection;
extern RuntimeConfig runtimeCfg;
extern LogCapture Log;

#endif // GLOBALS_H
