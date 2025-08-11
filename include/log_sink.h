#ifndef LOG_SINK_H
#define LOG_SINK_H

#include <Arduino.h>
#include <functional>
#include <string>

namespace logging
{
// Forward declarations
enum class LogLevel;
enum class LogFacility;

class log_sink
{
public:
    virtual ~log_sink() = default;
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
}

#endif
