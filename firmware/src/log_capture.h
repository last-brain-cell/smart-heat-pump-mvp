/**
 * @file log_capture.h
 * @brief Print subclass that tees output to both Serial and a ring buffer
 *
 * Replaces direct Serial usage across the codebase. Every byte written
 * goes to the real HardwareSerial AND a 4KB ring buffer that the
 * dashboard log viewer reads via /api/log.
 */

#ifndef LOG_CAPTURE_H
#define LOG_CAPTURE_H

#include <Arduino.h>

#define LOG_RING_SIZE 4096

class LogCapture : public Print {
    HardwareSerial& _serial;
    char _ring[LOG_RING_SIZE];
    volatile size_t _head;   // next write position (monotonic)
public:
    LogCapture(HardwareSerial& s);
    void begin(unsigned long baud);
    size_t write(uint8_t c) override;
    size_t write(const uint8_t* buf, size_t size) override;
    size_t getHead();
    size_t readLog(char* out, size_t outSize, size_t fromPos);
};

#endif // LOG_CAPTURE_H
