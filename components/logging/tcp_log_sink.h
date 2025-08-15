#ifndef TCP_LOG_SINK_H
#define TCP_LOG_SINK_H

#include "log_sink.h"
#include "log_serializers.h"
#include <map>

namespace logging {

/**
 * TCP log sink for sending data to TCP servers
 * Supports both client and server (listening) modes
 * Handles connection management and reconnection
 */
class TCPLogSink : public LogSink {
public:
    enum class Mode {
        CLIENT,      // Connect to remote TCP server
        SERVER       // Listen for incoming connections
    };

    TCPLogSink();
    ~TCPLogSink() override;

    bool init(const std::string& config) override;
    bool send(const output::BMSSnapshot& data) override;
    void shutdown() override;
    const char* getName() const override;
    bool isReady() const override;

    // TCP-specific operations
    bool connect();
    bool listen();
    bool reconnect();

private:
    BMSSerializer* serializer_;
    int socket_fd_;
    bool initialized_;
    Mode mode_;

    // Configuration
    struct Config {
        std::string host = "";     // "192.168.1.100"
        int port = 3331;
        std::string format = "json";
        int reconnect_interval_ms = 5000;
        bool auto_reconnect = true;
        Mode mode = Mode::CLIENT;
        int max_connections = 1;  // For server mode
    } config_;

    // State management
    bool is_connected_;
    uint32_t last_attempt_ms_;

    bool parseConfig(const std::string& config_str);
    bool createSocket();
    void closeSocket();
    void closeClient(int client_fd);
    
    bool handleClientConnection(int client_fd);
    bool sendToClient(int client_fd, const std::string& data);
    
    // Stats
    size_t total_bytes_sent_;
    size_t connections_count_;
    size_t reconnections_count_;
};

} // namespace logging

#endif // TCP_LOG_SINK_H