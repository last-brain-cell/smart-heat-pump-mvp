/*
 * gsm.h - GSM Communication Module
 *
 * Handles SIM800C initialization, SMS send/receive, and GPRS connection
 * using the TinyGSM library.
 */

#ifndef GSM_H
#define GSM_H

#include <Arduino.h>

// Define modem type BEFORE including TinyGSM
#define TINY_GSM_MODEM_SIM800
#define TINY_GSM_RX_BUFFER 256

#include <TinyGsmClient.h>
#include "config.h"
#include "types.h"

// ============================================================================
// GLOBAL OBJECTS (defined in main .ino file)
// ============================================================================
extern TinyGsm modem;
extern TinyGsmClient gsmClient;
extern GSMState gsmState;

// ============================================================================
// FUNCTION DECLARATIONS
// ============================================================================
bool initGSM();
bool waitForNetwork(unsigned long timeout = NETWORK_TIMEOUT);
bool connectGPRS();
void disconnectGPRS();

bool sendSMS(const char* phone, const String& message);
bool checkIncomingSMS(SMSMessage& msg);
SMSCommand parseSMSCommand(const String& message);
void deleteAllSMS();

bool isNetworkConnected();
bool isGPRSConnected();
int getSignalQuality();
String getOperatorName();

// ============================================================================
// INITIALIZE GSM MODULE
// ============================================================================
bool initGSM() {
    Serial.println(F("[GSM] Initializing SIM800C..."));
    gsmState = GSM_INITIALIZING;

    // Initialize serial for GSM module
    Serial2.begin(GSM_BAUD, SERIAL_8N1, PIN_GSM_RX, PIN_GSM_TX);
    delay(3000);  // Give module time to boot

    // Try to communicate with module
    Serial.println(F("[GSM] Testing AT commands..."));

    // Restart modem (can take 10+ seconds)
    if (!modem.restart()) {
        Serial.println(F("[GSM] Failed to restart modem"));
        // Try init instead (faster but less thorough)
        if (!modem.init()) {
            Serial.println(F("[GSM] Failed to initialize modem"));
            gsmState = GSM_ERROR;
            return false;
        }
    }

    // Get modem info
    String modemInfo = modem.getModemInfo();
    Serial.print(F("[GSM] Modem: "));
    Serial.println(modemInfo);

    // Unlock SIM if PIN is set
    if (strlen(GSM_PIN) > 0 && modem.getSimStatus() != 3) {
        Serial.println(F("[GSM] Unlocking SIM..."));
        modem.simUnlock(GSM_PIN);
    }

    gsmState = GSM_READY;
    Serial.println(F("[GSM] Module initialized"));
    return true;
}

// ============================================================================
// WAIT FOR NETWORK REGISTRATION
// ============================================================================
bool waitForNetwork(unsigned long timeout) {
    Serial.println(F("[GSM] Waiting for network..."));

    unsigned long start = millis();
    while (millis() - start < timeout) {
        if (modem.isNetworkConnected()) {
            Serial.println(F("[GSM] Network connected!"));

            // Print operator and signal quality
            String op = getOperatorName();
            int signal = getSignalQuality();
            Serial.print(F("[GSM] Operator: ")); Serial.println(op);
            Serial.print(F("[GSM] Signal: ")); Serial.print(signal); Serial.println(F("%"));

            return true;
        }
        Serial.print(F("."));
        delay(1000);
    }

    Serial.println(F("\n[GSM] Network timeout!"));
    return false;
}

// ============================================================================
// CONNECT TO GPRS
// ============================================================================
bool connectGPRS() {
    if (!isNetworkConnected()) {
        Serial.println(F("[GSM] No network, cannot connect GPRS"));
        return false;
    }

    if (isGPRSConnected()) {
        Serial.println(F("[GSM] GPRS already connected"));
        return true;
    }

    Serial.println(F("[GSM] Connecting to GPRS..."));
    gsmState = GSM_CONNECTING_GPRS;

    // Connect with APN settings
    if (!modem.gprsConnect(APN, GPRS_USER, GPRS_PASS)) {
        Serial.println(F("[GSM] GPRS connection failed"));
        gsmState = GSM_READY;
        return false;
    }

    // Get IP address
    String ip = modem.getLocalIP();
    Serial.print(F("[GSM] GPRS connected, IP: "));
    Serial.println(ip);

    gsmState = GSM_GPRS_CONNECTED;
    return true;
}

// ============================================================================
// DISCONNECT FROM GPRS
// ============================================================================
void disconnectGPRS() {
    if (isGPRSConnected()) {
        modem.gprsDisconnect();
        Serial.println(F("[GSM] GPRS disconnected"));
    }
    gsmState = GSM_READY;
}

// ============================================================================
// SEND SMS
// ============================================================================
bool sendSMS(const char* phone, const String& message) {
    Serial.print(F("[GSM] Sending SMS to "));
    Serial.println(phone);
    Serial.print(F("[GSM] Message: "));
    Serial.println(message);

    bool success = modem.sendSMS(phone, message);

    if (success) {
        Serial.println(F("[GSM] SMS sent successfully"));
    } else {
        Serial.println(F("[GSM] SMS send failed!"));
    }

    return success;
}

// ============================================================================
// CHECK FOR INCOMING SMS
// Returns true if a new SMS was received
// ============================================================================
bool checkIncomingSMS(SMSMessage& msg) {
    // Use AT commands to check for SMS
    // TinyGSM doesn't have built-in SMS reading, so we use direct AT commands

    modem.stream.println("AT+CMGF=1");  // Set text mode
    delay(100);
    modem.stream.println("AT+CMGL=\"REC UNREAD\"");  // List unread messages
    delay(500);

    String response = "";
    while (modem.stream.available()) {
        response += (char)modem.stream.read();
    }

    // Check if we got a message
    // Response format: +CMGL: <index>,"REC UNREAD","<phone>",,"<timestamp>"
    //                  <message text>
    if (response.indexOf("+CMGL:") >= 0) {
        // Parse the response
        int phoneStart = response.indexOf("\",\"") + 3;
        int phoneEnd = response.indexOf("\"", phoneStart);
        if (phoneStart > 2 && phoneEnd > phoneStart) {
            msg.sender = response.substring(phoneStart, phoneEnd);
        }

        // Get message text (after the header line)
        int msgStart = response.indexOf("\r\n", response.indexOf("+CMGL:")) + 2;
        int msgEnd = response.indexOf("\r\n", msgStart);
        if (msgStart > 1 && msgEnd > msgStart) {
            msg.content = response.substring(msgStart, msgEnd);
            msg.content.trim();
        }

        msg.isNew = true;

        Serial.print(F("[GSM] SMS from: "));
        Serial.println(msg.sender);
        Serial.print(F("[GSM] Content: "));
        Serial.println(msg.content);

        // Delete read messages to free memory
        deleteAllSMS();

        return true;
    }

    return false;
}

// ============================================================================
// PARSE SMS COMMAND
// ============================================================================
SMSCommand parseSMSCommand(const String& message) {
    String cmd = message;
    cmd.trim();
    cmd.toUpperCase();

    if (cmd == "STATUS" || cmd == "STAT") {
        return SMS_CMD_STATUS;
    }
    if (cmd == "RESET" || cmd == "REBOOT" || cmd == "RESTART") {
        return SMS_CMD_RESET;
    }

    return SMS_CMD_UNKNOWN;
}

// ============================================================================
// DELETE ALL SMS MESSAGES
// ============================================================================
void deleteAllSMS() {
    modem.stream.println("AT+CMGF=1");  // Text mode
    delay(100);
    modem.stream.println("AT+CMGD=1,4");  // Delete all messages
    delay(500);

    // Clear any response
    while (modem.stream.available()) {
        modem.stream.read();
    }

    Serial.println(F("[GSM] SMS storage cleared"));
}

// ============================================================================
// HELPER FUNCTIONS
// ============================================================================

bool isNetworkConnected() {
    return modem.isNetworkConnected();
}

bool isGPRSConnected() {
    return modem.isGprsConnected();
}

int getSignalQuality() {
    int rssi = modem.getSignalQuality();
    // Convert RSSI to percentage (0-31 scale, 99 = unknown)
    if (rssi == 99) return 0;
    return map(rssi, 0, 31, 0, 100);
}

String getOperatorName() {
    return modem.getOperator();
}

// ============================================================================
// FORMAT STATUS MESSAGE FOR SMS
// ============================================================================
String formatStatusMessage(const SystemData& data) {
    String msg = "Heat Pump Status\n";
    msg += "================\n";

    msg += "Temps(C):\n";
    msg += " In:" + String(data.tempInlet.value, 1);
    msg += " Out:" + String(data.tempOutlet.value, 1) + "\n";
    msg += " Amb:" + String(data.tempAmbient.value, 1);
    msg += " Comp:" + String(data.tempCompressor.value, 1) + "\n";

    msg += "Elec:\n";
    msg += " " + String(data.voltage.value, 0) + "V";
    msg += " " + String(data.current.value, 1) + "A";
    msg += " " + String(data.power, 0) + "W\n";

    msg += "Press(PSI):\n";
    msg += " Hi:" + String(data.pressureHigh.value, 0);
    msg += " Lo:" + String(data.pressureLow.value, 0) + "\n";

    msg += "Comp:" + String(data.compressorRunning ? "ON" : "OFF");

    return msg;
}

#endif // GSM_H
