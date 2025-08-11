#ifndef LOG_SINK_H
#define LOG_SINK_H

#include <Arduino.h>
#include <functional>
#include <string>

namespace applog {

// Forward declarations
enum class LogLevel;
enum class LogFacility;

class LogSink
{
public:
    virtual ~LogSink() = default;
    virtual void begin() = 0;
    virtual void tick() {}
    virtual void write(const String& line) = 0;
    
    // New interface for the unified logger
    virtual void write(LogLevel level, LogFacility facility, const String& message) {
        // Default implementation for backward compatibility
        (void)level;
        (void)facility;
        write(message);
    }
};

} // namespace applog

#endif // LOG_SINK_H