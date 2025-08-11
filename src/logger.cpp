#include "logger.h"
#include "log_sink.h"
#include <cstdarg>
#include <memory>

namespace logging {

Logger& Logger::getInstance() {
    static Logger instance;
    return instance;
}

Logger::Logger() : minLevel_(LogLevel::INFO) {
    // Enable all facilities by default
    enabledFacilities_.set();
}

void Logger::setLogLevel(LogLevel level) {
    minLevel_ = level;
}

void Logger::enableFacility(LogFacility facility, bool enabled) {
    enabledFacilities_.set(static_cast<size_t>(facility), enabled);
}

bool Logger::isFacilityEnabled(LogFacility facility) const {
    return enabledFacilities_.test(static_cast<size_t>(facility));
}

bool Logger::isLevelEnabled(LogLevel level) const {
    return static_cast<int>(level) >= static_cast<int>(minLevel_);
}

const char* Logger::getFacilityName(LogFacility facility) const {
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

const char* Logger::getLevelName(LogLevel level) const {
    switch (level) {
        case LogLevel::DEBUG: return "DEBUG";
        case LogLevel::INFO: return "INFO";
        case LogLevel::WARN: return "WARN";
        case LogLevel::ERROR: return "ERROR";
        default: return "UNKNOWN";
    }
}

void Logger::log(LogLevel level, LogFacility facility, const char* format, ...) {
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

void Logger::addSink(std::unique_ptr<log_sink> sink) {
    if (sink) {
        sinks_.push_back(std::move(sink));
    }
}

} // namespace logging