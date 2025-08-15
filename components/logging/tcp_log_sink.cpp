#include "tcp_log_sink.h"
#include <tuple>
#include <iostream>
#include <stdlib.h>
#include <stdio.h>
#include <string>
#include <algorithm>

// ESP-IDF / POSIX includes
#include <esp_log.h>
#include <esp_netif.h>
#include <errno.h>
#include <sys/time.h>

using namespace logging;

TCPLogSink::TCPLogSink() :
    serializer_(nullptr),
    socket_fd_(-1),
    initialized_(false),
    mode_(Mode::CLIENT),
    is_connected_(false),
    last_attempt_ms_(0),
    total_bytes_sent_(0),
    connections_count_(0),
    reconnections_count_(0)
{
    setLastError("");
}

TCPLogSink::~TCPLogSink() {
    shutdown();
    // For now, we're not allocating dest_addr_ so we don't need to delete it
}

bool TCPLogSink::init(const std::string& config) {
    if (!parseConfig(config)) {
        setLastError("Failed to parse configuration");
        return false;
    }

    // Create serializer
    serializer_ = logging::BMSSerializer::createSerializer(config_.format);
    if (!serializer_) {
        setLastError("Failed to create serializer for format: " + config_.format);
        return false;
    }

    mode_ = config_.mode;

    // Create socket
    if (!createSocket()) {
        return false;
    }

    if (mode_ == Mode::CLIENT) {
        // For now, we'll just return false since we're not implementing TCP sockets
        setLastError("TCP client mode not implemented for ESP-IDF");
        return false;
    } else { // SERVER mode
        return listen();
    }

    initialized_ = true;
    return true;
}

bool TCPLogSink::send(const output::BMSSnapshot& data) {
    if (!initialized_ || !isReady()) {
        return false;
    }

    std::string serialized;
    if (!serializer_->serialize(data, serialized)) {
        setLastError("Failed to serialize data");
        return false;
    }

    bool success = false;

    // For now, we'll just return false since we're not implementing TCP sockets
    setLastError("TCP socket not implemented for ESP-IDF");
    return false;
}

void TCPLogSink::shutdown() {
    // Close client sockets
    // For now, we're not managing any client sockets

    closeSocket();
    serializer_.reset();
    initialized_ = false;
}

const char* TCPLogSink::getName() const {
    return "tcp";
}

bool TCPLogSink::isReady() const {
    return initialized_ && socket_fd_ >= 0;
}

bool TCPLogSink::connect() {
    if (is_connected_) {
        return true;
    }

    // For now, we'll just return false since we're not implementing TCP sockets
    setLastError("TCP connect not implemented for ESP-IDF");
    return false;
}

bool TCPLogSink::listen() {
    if (socket_fd_ < 0) {
        if (!createSocket()) {
            return false;
        }
    }

    // For now, we'll just return false since we're not implementing TCP sockets
    setLastError("TCP listen not implemented for ESP-IDF");
    return false;
}

bool TCPLogSink::parseConfig(const std::string& config_str) {
    std::string config = config_str + ",";  // Sentinel

    size_t start = 0;
    size_t pos = config.find('=');

    while (pos != std::string::npos) {
        size_t next_comma = config.find(',', pos);
        size_t prev_comma = config.rfind(',', pos-1);

        std::string key = config.substr(prev_comma+1, pos-prev_comma-1);
        std::string value = config.substr(pos+1, next_comma-pos-1);

        auto first_non_space = key.find_first_not_of(" \t\r\n");
        auto last_non_space = key.find_last_not_of(" \t\r\n");
        if (first_non_space != std::string::npos) {
          key = key.substr(first_non_space, last_non_space - first_non_space + 1);
        }

        first_non_space = value.find_first_not_of(" \t\r\n");
        last_non_space = value.find_last_not_of(" \t\r\n");
        if (first_non_space != std::string::npos) {
            value = value.substr(first_non_space, last_non_space - first_non_space + 1);
        }

        if (!value.empty() && value.front() == '"' && value.back() == '"') {
            value = value.substr(1, value.length()-2);
        }

        if (key == "host") config_.host = value;
        else if (key == "port") config_.port = atoi(value.c_str());
        else if (key == "format") config_.format = value;
        else if (key == "mode") {
            if (value == "client") mode_ = Mode::CLIENT;
            else if (value == "server") mode_ = Mode::SERVER;
            else mode_ = Mode::CLIENT;
        }
        else if (key == "reconnect_interval_ms") config_.reconnect_interval_ms = atoi(value.c_str());
        else if (key == "auto_reconnect") config_.auto_reconnect = (value == "true");
        else if (key == "max_connections") config_.max_connections = atoi(value.c_str());

        start = next_comma + 1;
        pos = config.find('=', start);
        if (next_comma+1 >= config.length()) break;
    }
    return true;
}

bool TCPLogSink::createSocket() {
    // ESP-IDF doesn't have standard POSIX socket functions
    // We'll need to use ESP-IDF specific APIs
    setLastError("TCP socket not implemented for ESP-IDF");
    return false;
}

void TCPLogSink::closeSocket() {
    // Close socket
    // For ESP-IDF, we would use appropriate APIs
    socket_fd_ = -1;
    is_connected_ = false;
}

void TCPLogSink::closeClient(int client_fd) {
    // Close client socket
    // For ESP-IDF, we would use appropriate APIs
}

bool TCPLogSink::handleClientConnection(int client_fd) {
    // TODO: Implement client connection handling
    return true;
}

bool TCPLogSink::sendToClient(int client_fd, const std::string& data) {
    // For now, we'll just return false since we're not implementing TCP sockets
    setLastError("TCP sendToClient not implemented for ESP-IDF");
    return false;
}
