#ifndef LOG_SINK_H
#define LOG_SINK_H

#include <memory>
#include <string>
#include <vector>
#include "bms_snapshot.h"

namespace logging {

/**
 * Base interface for log sinks
 */
class LogSink {
public:
    virtual ~LogSink() = default;

    /**
     * Initialize the sink with given configuration
     * @param config JSON-style configuration string
     * @return true if initialization succeeded
     */
    virtual bool init(const std::string& config) = 0;

    /**
     * Send log data to the destination
     * @param data BMS snapshot data
     * @return true if send succeeded
     */
    virtual bool send(const output::BMSSnapshot& data) = 0;

    /**
     * Shutdown the sink and release resources
     */
    virtual void shutdown() = 0;

    /**
     * Get sink name for identification
     * @return sink type name
     */
    virtual const char* getName() const = 0;

    /**
     * Check if sink is ready for transmission
     * @return true if ready
     */
    virtual bool isReady() const = 0;

    /**
     * Get last error message
     * @return error string if any
     */
    virtual std::string getLastError() const { return last_error_; }

protected:
    void setLastError(const std::string& err) { last_error_ = err; }

private:
    std::string last_error_;
};

// Use smart pointer for automatic memory management
using LogSinkPtr = std::unique_ptr<LogSink>;

} // namespace logging

#endif // LOG_SINK_H