/**
 * @file dashboard.h
 * @brief Live status dashboard web server
 *
 * Hosts a dark-themed HTML dashboard on port 80 when WiFi is connected.
 * Provides real-time sensor data, alert status, and system health via
 * a JSON API endpoint polled by the browser every 3 seconds.
 */

#ifndef DASHBOARD_H
#define DASHBOARD_H

#include <Arduino.h>

/**
 * @brief Start the dashboard WiFiServer on port 80
 * Call after WiFi connects successfully.
 */
void initDashboard();

/**
 * @brief Handle incoming HTTP clients (non-blocking)
 * Call every loop iteration. Returns immediately if no client.
 */
void handleDashboard();

/**
 * @brief Stop the dashboard server
 * Call when WiFi drops.
 */
void stopDashboard();

#endif // DASHBOARD_H
