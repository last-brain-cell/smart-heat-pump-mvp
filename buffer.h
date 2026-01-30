/*
 * buffer.h - Circular Data Buffer
 *
 * Stores sensor readings when MQTT/GPRS is unavailable.
 * Uses a circular buffer to store up to BUFFER_SIZE readings.
 */

#ifndef BUFFER_H
#define BUFFER_H

#include <Arduino.h>
#include "config.h"
#include "types.h"

// ============================================================================
// DATA BUFFER STRUCTURE
// ============================================================================
struct DataBuffer {
    SystemData readings[BUFFER_SIZE];
    uint8_t head;       // Next write position
    uint8_t tail;       // Next read position
    uint8_t count;      // Current number of items
    bool overflow;      // True if buffer has overflowed (oldest data lost)
};

// ============================================================================
// GLOBAL BUFFER INSTANCE
// ============================================================================
static DataBuffer dataBuffer;

// ============================================================================
// FUNCTION DECLARATIONS
// ============================================================================
void initBuffer();
bool bufferData(const SystemData& data);
bool bufferHasData();
uint8_t bufferCount();
SystemData* getNextBufferedData();
void markDataPublished();
void clearBuffer();
bool isBufferFull();
bool didBufferOverflow();
void resetOverflowFlag();

// ============================================================================
// INITIALIZE BUFFER
// ============================================================================
void initBuffer() {
    dataBuffer.head = 0;
    dataBuffer.tail = 0;
    dataBuffer.count = 0;
    dataBuffer.overflow = false;

    Serial.print(F("[BUFFER] Initialized, capacity: "));
    Serial.println(BUFFER_SIZE);
}

// ============================================================================
// ADD DATA TO BUFFER
// Returns true if added successfully
// If buffer is full, overwrites oldest data and sets overflow flag
// ============================================================================
bool bufferData(const SystemData& data) {
    if (dataBuffer.count >= BUFFER_SIZE) {
        // Buffer full - overwrite oldest data
        dataBuffer.overflow = true;
        dataBuffer.tail = (dataBuffer.tail + 1) % BUFFER_SIZE;
        dataBuffer.count--;

        Serial.println(F("[BUFFER] Overflow - oldest data overwritten"));
    }

    // Add new data at head position
    dataBuffer.readings[dataBuffer.head] = data;
    dataBuffer.head = (dataBuffer.head + 1) % BUFFER_SIZE;
    dataBuffer.count++;

    return true;
}

// ============================================================================
// CHECK IF BUFFER HAS DATA
// ============================================================================
bool bufferHasData() {
    return dataBuffer.count > 0;
}

// ============================================================================
// GET CURRENT BUFFER COUNT
// ============================================================================
uint8_t bufferCount() {
    return dataBuffer.count;
}

// ============================================================================
// GET POINTER TO NEXT BUFFERED DATA (oldest first)
// Returns nullptr if buffer is empty
// Does NOT remove the data - call markDataPublished() after successful send
// ============================================================================
SystemData* getNextBufferedData() {
    if (dataBuffer.count == 0) {
        return nullptr;
    }

    return &dataBuffer.readings[dataBuffer.tail];
}

// ============================================================================
// MARK OLDEST DATA AS PUBLISHED (remove from buffer)
// Call this after successfully publishing data
// ============================================================================
void markDataPublished() {
    if (dataBuffer.count > 0) {
        dataBuffer.tail = (dataBuffer.tail + 1) % BUFFER_SIZE;
        dataBuffer.count--;
    }
}

// ============================================================================
// CLEAR ALL BUFFERED DATA
// ============================================================================
void clearBuffer() {
    dataBuffer.head = 0;
    dataBuffer.tail = 0;
    dataBuffer.count = 0;
    dataBuffer.overflow = false;

    Serial.println(F("[BUFFER] Cleared"));
}

// ============================================================================
// CHECK IF BUFFER IS FULL
// ============================================================================
bool isBufferFull() {
    return dataBuffer.count >= BUFFER_SIZE;
}

// ============================================================================
// CHECK IF BUFFER HAS OVERFLOWED
// ============================================================================
bool didBufferOverflow() {
    return dataBuffer.overflow;
}

// ============================================================================
// RESET OVERFLOW FLAG
// Call after acknowledging the overflow condition
// ============================================================================
void resetOverflowFlag() {
    dataBuffer.overflow = false;
}

// ============================================================================
// GET BUFFER STATUS STRING (for debugging/logging)
// ============================================================================
String getBufferStatus() {
    String status = "Buffer: ";
    status += String(dataBuffer.count);
    status += "/";
    status += String(BUFFER_SIZE);

    if (dataBuffer.overflow) {
        status += " (OVERFLOW)";
    }

    return status;
}

// ============================================================================
// PRINT BUFFER STATUS TO SERIAL
// ============================================================================
void printBufferStatus() {
    Serial.print(F("[BUFFER] Count: "));
    Serial.print(dataBuffer.count);
    Serial.print(F("/"));
    Serial.print(BUFFER_SIZE);
    Serial.print(F(" | Head: "));
    Serial.print(dataBuffer.head);
    Serial.print(F(" | Tail: "));
    Serial.print(dataBuffer.tail);
    if (dataBuffer.overflow) {
        Serial.print(F(" | OVERFLOW!"));
    }
    Serial.println();
}

#endif // BUFFER_H
