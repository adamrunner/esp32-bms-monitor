#ifndef HTTP_LOG_SINK_H
#define HTTP_LOG_SINK_H

#include "log_sink.h"
#include "log_serializers.h"
#include <memory>
#include <map>

namespace logging {

/**
 * HTTP log sink for POSTing data to web endpoints
 * Supports REST API endpoints, webhooks, etc.
 */
class HTTPLogSink : public LogSink {
public:
    HTTPLogSink();
    ~HTTPLogSink() override;

    bool init(const std::string& config) override;
    bool send(const output::BMSSnapshot& data) override;
    void shutdown() override;
    const char* getName() const override;
    bool isReady() const override;

private:
    BMSSerializer* serializer_;
    std::string url_;
    std::string method_;
    std::map<std::string, std::string> headers_;
    std::string auth_token_;
    int timeout_ms_;
    bool initialized_;

    // Configuration
    struct Config {
        std::string format = "json";
    } config_;

    bool parseConfig(const std::string& config_str);
    bool sendRequest(const std::string& data, const std::string& content_type);
    
    // Stats
    size_t requests_sent_;
    size_t bytes_sent_;
    size_t errors_;
    uint32_t last_success_ms_;
};

} // namespace logging

#endif // HTTP_LOG_SINK_H