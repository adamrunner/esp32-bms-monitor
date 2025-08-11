#ifndef LOG_SINK_H
#define LOG_SINK_H

#include <Arduino.h>
#include <functional>
#include <string>

namespace logging
{
class log_sink
{
public:
    virtual ~log_sink() = default;
    virtual void begin() = 0;
    virtual void tick() {}
    virtual void write(const String& line) = 0;
};
}

#endif
