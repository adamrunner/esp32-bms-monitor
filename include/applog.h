#ifndef APPLOG_H
#define APPLOG_H

#include <Arduino.h>
#include <bitset>
#include <vector>
#include <memory>

namespace applog {

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
class LogSink;

class AppLogger {
public:
    static AppLogger& getInstance();
    
    void log(LogLevel level, LogFacility facility, const char* format, ...);
    void setLogLevel(LogLevel level);
    void enableFacility(LogFacility facility, bool enabled);
    void addSink(std::unique_ptr<LogSink> sink);
    bool isFacilityEnabled(LogFacility facility) const;
    bool isLevelEnabled(LogLevel level) const;
    
    const char* getFacilityName(LogFacility facility) const;
    const char* getLevelName(LogLevel level) const;

private:
    AppLogger();
    ~AppLogger() = default;
    
    LogLevel minLevel_;
    std::bitset<8> enabledFacilities_;
    std::vector<std::unique_ptr<LogSink>> sinks_;
};

// Convenience macros for easier logging
#define APPLOG_DEBUG(facility, ...) applog::AppLogger::getInstance().log(applog::LogLevel::DEBUG, facility, __VA_ARGS__)
#define APPLOG_INFO(facility, ...) applog::AppLogger::getInstance().log(applog::LogLevel::INFO, facility, __VA_ARGS__)
#define APPLOG_WARN(facility, ...) applog::AppLogger::getInstance().log(applog::LogLevel::WARN, facility, __VA_ARGS__)
#define APPLOG_ERROR(facility, ...) applog::AppLogger::getInstance().log(applog::LogLevel::ERROR, facility, __VA_ARGS__)

} // namespace applog

#include "log_sink.h"

#endif // APPLOG_H