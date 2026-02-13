/**
 * @file log_capture.cpp
 * @brief LogCapture ring buffer implementation
 */

#include "log_capture.h"

LogCapture::LogCapture(HardwareSerial& s) : _serial(s), _head(0) {}

void LogCapture::begin(unsigned long baud) {
    _serial.begin(baud);
}

size_t LogCapture::write(uint8_t c) {
    _serial.write(c);
    _ring[_head % LOG_RING_SIZE] = (char)c;
    _head++;
    return 1;
}

size_t LogCapture::write(const uint8_t* buf, size_t size) {
    _serial.write(buf, size);
    for (size_t i = 0; i < size; i++) {
        _ring[(_head + i) % LOG_RING_SIZE] = (char)buf[i];
    }
    _head += size;
    return size;
}

size_t LogCapture::getHead() {
    return _head;
}

size_t LogCapture::readLog(char* out, size_t outSize, size_t fromPos) {
    size_t head = _head;

    // Nothing new
    if (fromPos >= head) return 0;

    size_t available = head - fromPos;

    // If fromPos is too old (data overwritten), start from oldest available
    if (available > LOG_RING_SIZE) {
        fromPos = head - LOG_RING_SIZE;
        available = LOG_RING_SIZE;
    }

    // Clamp to output buffer size (leave room for null terminator)
    size_t toRead = available;
    if (toRead > outSize - 1) toRead = outSize - 1;

    // Copy bytes from ring buffer, handling wrap
    for (size_t i = 0; i < toRead; i++) {
        out[i] = _ring[(fromPos + i) % LOG_RING_SIZE];
    }
    out[toRead] = '\0';

    return toRead;
}
