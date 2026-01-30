/**
 * @file globals.h
 * @brief Central declarations for shared global objects and forward declarations
 *
 * This header provides extern declarations for all globally shared objects,
 * eliminating scattered extern statements throughout the codebase.
 * Include this file in any module that needs access to shared state.
 */

#ifndef GLOBALS_H
#define GLOBALS_H

#include <Arduino.h>

// TinyGSM configuration - must be defined before including TinyGsmClient.h
#define TINY_GSM_MODEM_SIM800
#define TINY_GSM_RX_BUFFER 256

#include <TinyGsmClient.h>
#include <PubSubClient.h>

#include "types.h"

// =============================================================================
// GLOBAL OBJECT DECLARATIONS
// =============================================================================

/** @brief GSM modem instance for cellular communication */
extern TinyGsm modem;

/** @brief GSM client for TCP/IP connections over GPRS */
extern TinyGsmClient gsmClient;

/** @brief Current GSM module state */
extern GSMState gsmState;

/** @brief MQTT client for broker communication */
extern PubSubClient mqtt;

/** @brief Latest sensor readings */
extern SystemData currentData;

/** @brief Network registration status */
extern bool networkReady;

/** @brief Startup sequence completion flag */
extern bool startupComplete;

#endif // GLOBALS_H
