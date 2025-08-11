#ifndef SERIAL_LOG_SINK_H
#define SERIAL_LOG_SINK_H

#include "log_sink.h"

namespace applog {

// Forward declarations
enum class LogLevel;
enum class LogFacility;

class SerialLogSink : public LogSink {
public:
    SerialLogSink();
    void begin() override;
    void write(const String& line) override;
    void write(LogLevel level, LogFacility facility, const String& message) override;
    
private:
    String formatMessage(LogLevel level, LogFacility facility, const String& message);
    bool initialized_;
};

} // namespace applog

#endif // SERIAL_LOG_SINK_H