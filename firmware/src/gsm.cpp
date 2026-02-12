/**
 * @file gsm.cpp
 * @brief GSM communication implementation
 */

#include "gsm.h"

// =============================================================================
// PRIVATE CONSTANTS
// =============================================================================

static const char AT_CMGF[] PROGMEM = "AT+CMGF=1";           // Text mode
static const char AT_CMGL[] PROGMEM = "AT+CMGL=\"REC UNREAD\"";  // List unread
static const char AT_CMGD[] PROGMEM = "AT+CMGD=1,4";         // Delete all

// =============================================================================
// PRIVATE HELPER FUNCTIONS
// =============================================================================

/**
 * @brief Safely find a substring and return its index
 * @param str String to search in
 * @param substr Substring to find
 * @param startFrom Starting position
 * @return Index of substring, or -1 if not found
 */
static int safeFindSubstring(const String& str, const char* substr, int startFrom = 0) {
    if (startFrom < 0 || startFrom >= (int)str.length()) {
        return -1;
    }
    return str.indexOf(substr, startFrom);
}

/**
 * @brief Safely extract substring with bounds checking
 * @param str Source string
 * @param startIdx Start index
 * @param endIdx End index
 * @param output Output string
 * @return true if extraction successful
 */
static bool safeSubstring(const String& str, int startIdx, int endIdx, String& output) {
    if (startIdx < 0 || endIdx < 0 || startIdx >= (int)str.length() ||
        endIdx > (int)str.length() || startIdx >= endIdx) {
        return false;
    }
    output = str.substring(startIdx, endIdx);
    return true;
}

// =============================================================================
// IMPLEMENTATION
// =============================================================================

bool initGSM() {
    Serial.println(F("[GSM] Initializing SIM800C..."));
    gsmState = GSM_INITIALIZING;

    // Initialize serial for GSM module
    Serial2.begin(GSM_BAUD, SERIAL_8N1, PIN_GSM_RX, PIN_GSM_TX);
    delay(3000);  // Give module time to boot

    Serial.println(F("[GSM] Testing AT commands..."));

    // Try to restart modem (can take 10+ seconds)
    if (!modem.restart()) {
        Serial.println(F("[GSM] Failed to restart modem"));
        // Try init instead (faster but less thorough)
        if (!modem.init()) {
            Serial.println(F("[GSM] Failed to initialize modem"));
            gsmState = GSM_STATE_ERROR;
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

bool waitForNetwork(unsigned long timeout) {
    Serial.println(F("[GSM] Waiting for network..."));

    unsigned long start = millis();
    while (millis() - start < timeout) {
        if (modem.isNetworkConnected()) {
            Serial.println(F("[GSM] Network connected!"));

            String op = getOperatorName();
            int signal = getSignalQuality();
            Serial.print(F("[GSM] Operator: "));
            Serial.println(op);
            Serial.print(F("[GSM] Signal: "));
            Serial.print(signal);
            Serial.println(F("%"));

            return true;
        }
        Serial.print(F("."));
        delay(1000);
    }

    Serial.println(F("\n[GSM] Network timeout!"));
    return false;
}

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

    if (!modem.gprsConnect(APN, GPRS_USER, GPRS_PASS)) {
        Serial.println(F("[GSM] GPRS connection failed"));
        gsmState = GSM_READY;
        return false;
    }

    String ip = modem.getLocalIP();
    Serial.print(F("[GSM] GPRS connected, IP: "));
    Serial.println(ip);

    gsmState = GSM_GPRS_CONNECTED;
    return true;
}

void disconnectGPRS() {
    if (isGPRSConnected()) {
        modem.gprsDisconnect();
        Serial.println(F("[GSM] GPRS disconnected"));
    }
    gsmState = GSM_READY;
}

bool sendSMS(const char* phone, const char* message) {
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

bool checkIncomingSMS(SMSMessage& msg) {
    // Set text mode
    modem.stream.println(F("AT+CMGF=1"));
    delay(100);

    // List unread messages
    modem.stream.println(F("AT+CMGL=\"REC UNREAD\""));
    delay(500);

    // Read response with timeout
    String response = "";
    unsigned long start = millis();
    while (millis() - start < 1000) {
        while (modem.stream.available()) {
            response += (char)modem.stream.read();
        }
        delay(10);
    }

    // Check if we got a message
    // Response format: +CMGL: <index>,"REC UNREAD","<phone>",,"<timestamp>"\r\n<message>\r\n
    int cmglPos = safeFindSubstring(response, "+CMGL:");
    if (cmglPos < 0) {
        return false;  // No unread messages
    }

    // Parse phone number - find the pattern: ","<phone>","
    // First occurrence of "," after +CMGL: is status, second is phone
    int firstQuote = safeFindSubstring(response, "\",\"", cmglPos);
    if (firstQuote < 0) {
        Serial.println(F("[GSM] SMS parse error: phone start not found"));
        deleteAllSMS();
        return false;
    }

    int phoneStart = firstQuote + 3;  // Skip past ",\"
    int phoneEnd = safeFindSubstring(response, "\"", phoneStart);
    if (phoneEnd < 0 || phoneEnd <= phoneStart) {
        Serial.println(F("[GSM] SMS parse error: phone end not found"));
        deleteAllSMS();
        return false;
    }

    if (!safeSubstring(response, phoneStart, phoneEnd, msg.sender)) {
        Serial.println(F("[GSM] SMS parse error: phone extraction failed"));
        deleteAllSMS();
        return false;
    }

    // Find message content - it's on the line after the header
    int headerEnd = safeFindSubstring(response, "\r\n", cmglPos);
    if (headerEnd < 0) {
        Serial.println(F("[GSM] SMS parse error: header end not found"));
        deleteAllSMS();
        return false;
    }

    int msgStart = headerEnd + 2;  // Skip past \r\n
    int msgEnd = safeFindSubstring(response, "\r\n", msgStart);
    if (msgEnd < 0) {
        // Message might be at the end without trailing \r\n
        msgEnd = response.length();
    }

    if (msgStart >= msgEnd || msgStart >= (int)response.length()) {
        Serial.println(F("[GSM] SMS parse error: message extraction failed"));
        deleteAllSMS();
        return false;
    }

    if (!safeSubstring(response, msgStart, msgEnd, msg.content)) {
        Serial.println(F("[GSM] SMS parse error: content extraction failed"));
        deleteAllSMS();
        return false;
    }

    msg.content.trim();
    msg.isNew = true;

    Serial.print(F("[GSM] SMS from: "));
    Serial.println(msg.sender);
    Serial.print(F("[GSM] Content: "));
    Serial.println(msg.content);

    // Delete read messages to free memory
    deleteAllSMS();

    return true;
}

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

void deleteAllSMS() {
    modem.stream.println(F("AT+CMGF=1"));  // Text mode
    delay(100);
    modem.stream.println(F("AT+CMGD=1,4"));  // Delete all messages
    delay(500);

    // Clear any response
    while (modem.stream.available()) {
        modem.stream.read();
    }

    Serial.println(F("[GSM] SMS storage cleared"));
}

bool isNetworkConnected() {
    return modem.isNetworkConnected();
}

bool isGPRSConnected() {
    return modem.isGprsConnected();
}

int getSignalQuality() {
    int rssi = modem.getSignalQuality();
    // Convert RSSI to percentage (0-31 scale, 99 = unknown)
    if (rssi == 99 || rssi < 0) {
        return 0;
    }
    return map(rssi, 0, 31, 0, 100);
}

String getOperatorName() {
    return modem.getOperator();
}

size_t formatStatusMessage(const SystemData& data, char* buffer, size_t bufferSize) {
    return snprintf(buffer, bufferSize,
        "Heat Pump Status\n"
        "================\n"
        "Temps(C):\n"
        " In:%.1f Out:%.1f\n"
        " Amb:%.1f Comp:%.1f\n"
        "Elec:\n"
        " %.0fV %.1fA %.0fW\n"
        "Press(PSI):\n"
        " Hi:%.0f Lo:%.0f\n"
        "Comp:%s",
        data.tempInlet.value,
        data.tempOutlet.value,
        data.tempAmbient.value,
        data.tempCompressor.value,
        data.voltage.value,
        data.current.value,
        data.power,
        data.pressureHigh.value,
        data.pressureLow.value,
        data.compressorRunning ? "ON" : "OFF"
    );
}
