#include "applog.h"
#include "log_sink.h"
#include <cstdarg>
#include <memory>

namespace applog {

AppLogger& AppLogger::getInstance() {
    static AppLogger instance;
    return instance;
}

AppLogger::AppLogger() : minLevel_(LogLevel::INFO) {
    // Enable all facilities by default
    enabledFacilities_.set();
}

void AppLogger::setLogLevel(LogLevel level) {
    minLevel_ = level;
}

void AppLogger::enableFacility(LogFacility facility, bool enabled) {
    enabledFacilities_.set(static_cast<size_t>(facility), enabled);
}

bool AppLogger::isFacilityEnabled(LogFacility facility) const {
    return enabledFacilities_.test(static_cast<size_t>(facility));
}

bool AppLogger::isLevelEnabled(LogLevel level) const {
    return static_cast<int>(level) >= static_cast<int>(minLevel_);
}

const char* AppLogger::getFacilityName(LogFacility facility) const {
    switch (facility) {
        case LogFacility::MAIN: return "MAIN";
        case LogFacility::MQTT: return "MQTT";
        case LogFacility::WIFI: return "WIFI";
        case LogFacility::BMS_COMM: return "BMS";
        case LogFacility::DATA_LOG: return "DATA";
        case LogFacility::SYSTEM: return "SYS";
        default: return "UNKNOWN";
    }
}

const char* AppLogger::getLevelName(LogLevel level) const {
    switch (level) {
        case LogLevel::DEBUG: return "DEBUG";
        case LogLevel::INFO: return "INFO";
        case LogLevel::WARN: return "WARN";
        case LogLevel::ERROR: return "ERROR";
        default: return "UNKNOWN";
    }
}

void AppLogger::log(LogLevel level, LogFacility facility, const char* format, ...) {
    // Check if this log should be processed
    if (!isLevelEnabled(level) || !isFacilityEnabled(facility)) {
        return;
    }
    
    // Format the message
    char buffer[512];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    
    String message(buffer);
    
    // Send to all sinks
    for (auto& sink : sinks_) {
        if (sink) {
            sink->write(level, facility, message);
        }
    }
}

void AppLogger::addSink(std::unique_ptr<LogSink> sink) {
    if (sink) {
        sinks_.push_back(std::move(sink));
    }
}

} // namespace applog