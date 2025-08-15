#ifndef UDP_LOG_SINK_H
#define UDP_LOG_SINK_H

#include "log_sink.h"
#include "log_serializers.h"

struct sockaddr_in;

namespace logging {

/**
 * UDP log sink for broadcasting/multicasting log data
 * Supports unicast, broadcast, and multicast transmission
 * Handles packet size limits and rate limiting
 */
class UDPLogSink : public LogSink {
public:
    UDPLogSink();
    ~UDPLogSink() override;

    bool init(const std::string& config) override;
    bool send(const output::BMSSnapshot& data) override;
    void shutdown() override;
    const char* getName() const override;
    bool isReady() const override;

private:
    BMSSerializer* serializer_;
    int socket_fd_;
    struct sockaddr_in* dest_addr_;
    bool initialized_;

    // Configuration
    struct Config {
        std::string ip = "255.255.255.255";  // Default to broadcast
        int port = 3330;
        bool broadcast = true;
        std::string format = "json";
        int max_packs_per_batch = 1;
        size_t max_packet_size = 1400;  // Conservative for typical network
    } config_;

    bool parseConfig(const std::string& config_str);
    bool createSocket();
    bool configureSocket();
    void closeSocket();
    
    // Stats
    size_t total_bytes_sent_;
    size_t packets_sent_;
    size_t errors_;
};

} // namespace logging

#endif // UDP_LOG_SINK_H