#include "serial_sink.h"
#include "logger.h"
#include <Arduino.h>

namespace logging {

serial_sink::serial_sink() : initialized_(false) {}

void serial_sink::begin() {
    // Serial is already initialized in main.cpp, so we just mark ourselves as ready
    initialized_ = true;
}

void serial_sink::write(const String& line) {
    if (!initialized_) return;
    Serial.println(line);
}

void serial_sink::write(LogLevel level, LogFacility facility, const String& message) {
    if (!initialized_) return;
    
    String formatted = formatMessage(level, facility, message);
    Serial.println(formatted);
}

String serial_sink::formatMessage(LogLevel level, LogFacility facility, const String& message) {
    String result = "[";
    result += Logger::getInstance().getFacilityName(facility);
    result += " ";
    result += Logger::getInstance().getLevelName(level);
    result += "] ";
    result += message;
    return result;
}

} // namespace logging