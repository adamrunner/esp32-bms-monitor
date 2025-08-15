#ifndef SERIAl_LOG_SINK_H
#define SERIAl_LOG_SINK_H

#include "log_sink.h"
#include "log_serializers.h"
#include <memory>

namespace logging {

/**
 * Serial/Console log sink that outputs to ESP32 serial port
 * Replaces the existing output.cpp functionality
 */
class SerialLogSink : public LogSink {
public:
    SerialLogSink();
    ~SerialLogSink() override;

    bool init(const std::string& config) override;
    bool send(const output::BMSSnapshot& data) override;
    void shutdown() override;
    const char* getName() const override;
    bool isReady() const override;

private:
    std::unique_ptr<BMSSerializer> serializer_;
    bool initialized_;
    bool config_print_header_;
    bool printed_header_;

    // Configuration options
    struct Config {
        std::string format = "human";  // human, csv, json
        bool print_header = true;
        int max_cells = 16;
        int max_temps = 8;
    } config_;

    bool parseConfig(const std::string& config_str);
};

} // namespace logging

#endif // SERIAl_LOG_SINK_H
