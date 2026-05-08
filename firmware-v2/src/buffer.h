/**
 * @file buffer.h
 * @brief Circular data buffer for offline storage
 *
 * Stores sensor readings when MQTT/GPRS is unavailable.
 * Uses a circular buffer to store up to BUFFER_SIZE readings,
 * with oldest data overwritten when full.
 */

#ifndef BUFFER_H
#define BUFFER_H

#include <Arduino.h>
#include "../config.h"
#include "types.h"

// =============================================================================
// DATA BUFFER STRUCTURE
// =============================================================================

/**
 * @brief Circular buffer for storing sensor readings
 */
struct DataBuffer {
    SystemData readings[BUFFER_SIZE];
    uint16_t head;      ///< Next write position
    uint16_t tail;      ///< Next read position
    uint16_t count;     ///< Current number of items
    bool overflow;      ///< True if buffer has overflowed (oldest data lost)
};

// =============================================================================
// FUNCTION DECLARATIONS
// =============================================================================

/**
 * @brief Initialize the data buffer
 */
void initBuffer();

/**
 * @brief Add sensor data to the buffer
 * @param data Sensor readings to store
 * @return true if added successfully
 * @note Overwrites oldest data if buffer is full
 */
bool bufferData(const SystemData& data);

/**
 * @brief Check if buffer contains data
 * @return true if buffer has at least one reading
 */
bool bufferHasData();

/**
 * @brief Get current number of buffered readings
 * @return Number of readings in buffer
 */
uint16_t bufferCount();

/**
 * @brief Get pointer to oldest buffered data (FIFO)
 * @return Pointer to data, or nullptr if buffer empty
 * @note Does not remove data - call markDataPublished() after successful send
 */
SystemData* getNextBufferedData();

/**
 * @brief Mark oldest data as published and remove from buffer
 * @note Call after successfully publishing data returned by getNextBufferedData()
 */
void markDataPublished();

/**
 * @brief Clear all buffered data
 */
void clearBuffer();

/**
 * @brief Check if buffer is at capacity
 * @return true if buffer is full
 */
bool isBufferFull();

/**
 * @brief Check if any data was lost due to overflow
 * @return true if overflow has occurred
 */
bool didBufferOverflow();

/**
 * @brief Reset the overflow flag
 * @note Call after acknowledging the overflow condition
 */
void resetOverflowFlag();

/**
 * @brief Get buffer status as formatted string
 * @param buffer Output buffer for status string
 * @param bufferSize Size of output buffer
 * @return Number of characters written
 */
size_t getBufferStatus(char* buffer, size_t bufferSize);

/**
 * @brief Print buffer status to serial monitor
 */
void printBufferStatus();

#endif // BUFFER_H
