#ifndef SERIAL_SINK_H
#define SERIAL_SINK_H

#include "log_sink.h"

namespace logging {

// Forward declarations
enum class LogLevel;
enum class LogFacility;

class serial_sink : public log_sink {
public:
    serial_sink();
    void begin() override;
    void write(const String& line) override;
    void write(LogLevel level, LogFacility facility, const String& message) override;
    
private:
    String formatMessage(LogLevel level, LogFacility facility, const String& message);
    bool initialized_;
};

} // namespace logging

#endif // SERIAL_SINK_H