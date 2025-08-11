#ifndef LOGGER_H
#define LOGGER_H

#include <Arduino.h>
#include <bitset>
#include <vector>
#include <memory>

namespace logging {

enum class LogLevel {
    DEBUG = 0,
    INFO = 1,
    WARN = 2,
    ERROR = 3
};

enum class LogFacility {
    MAIN = 0,      // Main application flow and general messages
    MQTT = 1,      // MQTT connectivity and messaging
    WIFI = 2,      // WiFi connectivity
    BMS_COMM = 3,  // BMS communication protocol details
    DATA_LOG = 4,  // Periodic data logging output (CSV/Human)
    SYSTEM = 5     // System-level messages (startup, config, etc.)
};

// Forward declaration
class log_sink;

class Logger {
public:
    static Logger& getInstance();
    
    void log(LogLevel level, LogFacility facility, const char* format, ...);
    void setLogLevel(LogLevel level);
    void enableFacility(LogFacility facility, bool enabled);
    void addSink(std::unique_ptr<log_sink> sink);
    bool isFacilityEnabled(LogFacility facility) const;
    bool isLevelEnabled(LogLevel level) const;
    
    const char* getFacilityName(LogFacility facility) const;
    const char* getLevelName(LogLevel level) const;

private:
    Logger();
    ~Logger() = default;
    
    LogLevel minLevel_;
    std::bitset<8> enabledFacilities_;
    std::vector<std::unique_ptr<log_sink>> sinks_;
};

// Convenience macros for easier logging
#define LOG_DEBUG(facility, ...) logging::Logger::getInstance().log(logging::LogLevel::DEBUG, facility, __VA_ARGS__)
#define LOG_INFO(facility, ...) logging::Logger::getInstance().log(logging::LogLevel::INFO, facility, __VA_ARGS__)
#define LOG_WARN(facility, ...) logging::Logger::getInstance().log(logging::LogLevel::WARN, facility, __VA_ARGS__)
#define LOG_ERROR(facility, ...) logging::Logger::getInstance().log(logging::LogLevel::ERROR, facility, __VA_ARGS__)

} // namespace logging

#include "log_sink.h"

#endif // LOGGER_H