#ifndef TIME_MANAGER_H
#define TIME_MANAGER_H

#include <Arduino.h>

namespace time_manager {

// Initialize the time manager
bool initialize();

// Update time from NTP server
bool syncTime();

// Check if time is synchronized
bool isTimeSynchronized();

// Get current Unix timestamp
unsigned long getUnixTimestamp();

// Get formatted time string
String getFormattedTime();

// Get ISO 8601 formatted timestamp
String getISOTimeString();

} // namespace time_manager

#endif // TIME_MANAGER_H