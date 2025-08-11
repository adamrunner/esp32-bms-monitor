#include "serial_log_sink.h"
#include "applog.h"
#include <cstdarg>

namespace applog {

SerialLogSink::SerialLogSink() : initialized_(false) {}

void SerialLogSink::begin() {
    // Serial is already initialized in main.cpp
    initialized_ = true;
}

void SerialLogSink::write(const String& line) {
    if (initialized_) {
        Serial.println(line);
    }
}

void SerialLogSink::write(LogLevel level, LogFacility facility, const String& message) {
    if (initialized_) {
        String formatted = formatMessage(level, facility, message);
        Serial.println(formatted);
    }
}

String SerialLogSink::formatMessage(LogLevel level, LogFacility facility, const String& message) {
    String formatted = "[";
    formatted += AppLogger::getInstance().getLevelName(level);
    formatted += "][";
    formatted += AppLogger::getInstance().getFacilityName(facility);
    formatted += "] ";
    formatted += message;
    return formatted;
}

} // namespace applog