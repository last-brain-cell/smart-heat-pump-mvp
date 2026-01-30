/**
 * @file buffer.cpp
 * @brief Circular data buffer implementation
 */

#include "buffer.h"

// =============================================================================
// PRIVATE DATA
// =============================================================================

static DataBuffer dataBuffer;

// =============================================================================
// IMPLEMENTATION
// =============================================================================

void initBuffer() {
    dataBuffer.head = 0;
    dataBuffer.tail = 0;
    dataBuffer.count = 0;
    dataBuffer.overflow = false;

    Serial.print(F("[BUFFER] Initialized, capacity: "));
    Serial.println(BUFFER_SIZE);
}

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

bool bufferHasData() {
    return dataBuffer.count > 0;
}

uint16_t bufferCount() {
    return dataBuffer.count;
}

SystemData* getNextBufferedData() {
    if (dataBuffer.count == 0) {
        return nullptr;
    }
    return &dataBuffer.readings[dataBuffer.tail];
}

void markDataPublished() {
    if (dataBuffer.count > 0) {
        dataBuffer.tail = (dataBuffer.tail + 1) % BUFFER_SIZE;
        dataBuffer.count--;
    }
}

void clearBuffer() {
    dataBuffer.head = 0;
    dataBuffer.tail = 0;
    dataBuffer.count = 0;
    dataBuffer.overflow = false;
    Serial.println(F("[BUFFER] Cleared"));
}

bool isBufferFull() {
    return dataBuffer.count >= BUFFER_SIZE;
}

bool didBufferOverflow() {
    return dataBuffer.overflow;
}

void resetOverflowFlag() {
    dataBuffer.overflow = false;
}

size_t getBufferStatus(char* buffer, size_t bufferSize) {
    if (dataBuffer.overflow) {
        return snprintf(buffer, bufferSize, "Buffer: %u/%d (OVERFLOW)",
                        dataBuffer.count, BUFFER_SIZE);
    }
    return snprintf(buffer, bufferSize, "Buffer: %u/%d",
                    dataBuffer.count, BUFFER_SIZE);
}

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
