/**
 * @file gsm.h
 * @brief GSM communication interface
 *
 * Handles SIM800C initialization, SMS send/receive, and GPRS connection
 * using the TinyGSM library.
 */

#ifndef GSM_H
#define GSM_H

#include <Arduino.h>
#include "../config.h"
#include "types.h"
#include "globals.h"

// =============================================================================
// FUNCTION DECLARATIONS
// =============================================================================

/**
 * @brief Initialize the GSM module
 * @return true if initialization successful
 */
bool initGSM();

/**
 * @brief Wait for network registration
 * @param timeout Maximum time to wait in milliseconds
 * @return true if network registered within timeout
 */
bool waitForNetwork(unsigned long timeout = NETWORK_TIMEOUT);

/**
 * @brief Connect to GPRS data network
 * @return true if GPRS connection established
 */
bool connectGPRS();

/**
 * @brief Disconnect from GPRS
 */
void disconnectGPRS();

/**
 * @brief Send an SMS message
 * @param phone Destination phone number (with country code)
 * @param message Message content
 * @return true if SMS sent successfully
 */
bool sendSMS(const char* phone, const char* message);

/**
 * @brief Check for incoming SMS messages
 * @param msg Output structure for received message
 * @return true if new message received
 */
bool checkIncomingSMS(SMSMessage& msg);

/**
 * @brief Parse SMS content to determine command
 * @param message SMS message content
 * @return Parsed command type
 */
SMSCommand parseSMSCommand(const String& message);

/**
 * @brief Delete all SMS messages from SIM storage
 */
void deleteAllSMS();

/**
 * @brief Check if registered on cellular network
 * @return true if network connected
 */
bool isNetworkConnected();

/**
 * @brief Check if GPRS data connection is active
 * @return true if GPRS connected
 */
bool isGPRSConnected();

/**
 * @brief Get signal quality as percentage
 * @return Signal strength 0-100%
 */
int getSignalQuality();

/**
 * @brief Get network operator name
 * @return Operator name string
 */
String getOperatorName();

/**
 * @brief Format system data as status message for SMS
 * @param data System data to format
 * @param buffer Output buffer
 * @param bufferSize Size of output buffer
 * @return Number of characters written
 */
size_t formatStatusMessage(const SystemData& data, char* buffer, size_t bufferSize);

#endif // GSM_H
