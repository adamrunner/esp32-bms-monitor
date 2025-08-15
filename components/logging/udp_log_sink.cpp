#include "udp_log_sink.h"
#include <tuple>
#include <iostream>
#include <stdlib.h>
#include <stdio.h>
#include <string>

// ESP-IDF / POSIX includes
#include <esp_log.h>
#include <esp_netif.h>

using namespace logging;

UDPLogSink::UDPLogSink() :
    serializer_(nullptr),
    socket_fd_(-1),
    dest_addr_(nullptr),
    initialized_(false),
    total_bytes_sent_(0),
    packets_sent_(0),
    errors_(0)
{
    setLastError("");
    // For now, we'll just set dest_addr_ to nullptr
    // In a real implementation, we'd need to allocate and initialize properly
}

UDPLogSink::~UDPLogSink() {
    shutdown();
    // For now, we're not allocating dest_addr_ so we don't need to delete it
}

bool UDPLogSink::init(const std::string& config) {
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

    // Create socket
    if (!createSocket()) {
        return false;
    }

    initialized_ = true;
    return true;
}

bool UDPLogSink::send(const output::BMSSnapshot& data) {
    if (!initialized_ || !isReady()) {
        return false;
    }

    std::string serialized;
    if (!serializer_->serialize(data, serialized)) {
        setLastError("Failed to serialize data");
        errors_++;
        return false;
    }

    // Check packet size
    if (serialized.length() > config_.max_packet_size) {
        setLastError("Data too large for UDP packet");
        errors_++;
        return false;
    }

    // For now, we'll just return false since we're not implementing UDP sockets
    setLastError("UDP socket not implemented for ESP-IDF");
    errors_++;
    return false;
}

void UDPLogSink::shutdown() {
    closeSocket();
    serializer_.reset();
    initialized_ = false;
}

const char* UDPLogSink::getName() const {
    return "udp";
}

bool UDPLogSink::isReady() const {
    return initialized_ && socket_fd_ >= 0;
}

bool UDPLogSink::parseConfig(const std::string& config_str) {
    // Key=value parser for "ip=192.168.1.255,port=3330,format=json"
    std::string config = config_str + ",";  // Sentinel

    size_t start = 0;
    size_t pos = config.find('=');

    while (pos != std::string::npos) {
        size_t next_comma = config.find(',', pos);
        size_t prev_comma = config.rfind(',', pos-1);

        std::string key = config.substr(prev_comma+1, pos-prev_comma-1);
        std::string value = config.substr(pos+1, next_comma-pos-1);

        // Trim whitespace
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

        // Strip quotes
        if (!value.empty() && value.front() == '"' && value.back() == '"') {
            value = value.substr(1, value.length()-2);
        }

        if (key == "ip") config_.ip = value;
        else if (key == "port") config_.port = atoi(value.c_str());
        else if (key == "format") config_.format = value;
        else if (key == "broadcast") config_.broadcast = (value == "true");
        else if (key == "max_packet_size") config_.max_packet_size = atoi(value.c_str());
        else if (key == "max_packs_per_batch") config_.max_packs_per_batch = atoi(value.c_str());

        start = next_comma + 1;
        pos = config.find('=', start);
    }
    return true;
}

bool UDPLogSink::createSocket() {
    // ESP-IDF doesn't have standard POSIX socket functions
    // We'll need to use ESP-IDF specific APIs
    setLastError("UDP socket not implemented for ESP-IDF");
    return false;
}

bool UDPLogSink::configureSocket() {
    // Set socket to non-blocking mode
    // For ESP-IDF, we would use appropriate APIs
    return true;
}

void UDPLogSink::closeSocket() {
    // Close socket
    // For ESP-IDF, we would use appropriate APIs
}
