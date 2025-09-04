#ifndef LOG_MANAGER_H
#define LOG_MANAGER_H

#include "log_sink.h"
<<<<<<< HEAD
#include "log_serializers.h"
=======
>>>>>>> esp-idf
#include "bms_snapshot.h"
#include <memory>
#include <vector>
#include <map>
#include <string>
#include <functional>

namespace logging {

/**
 * Central logging manager that coordinates multiple sinks
 * Singleton pattern ensures single point of control
 */
class LogManager {
public:
    /**
     * Get singleton instance
     * Singleton pattern ensures thread-safe lazy initialization
     */
    static LogManager& getInstance();

    // Prevent copying
    LogManager(const LogManager&) = delete;
    LogManager& operator=(const LogManager&) = delete;

    /**
     * Initialize the logging manager with configuration
     * @param config JSON string with sink configurations
     * @return true if all sinks initialized successfully
     */
    bool init(const std::string& config);

    /**
     * Send BMS data to all active sinks
     * @param data BMS snapshot to distribute
     * @return number of successful deliveries
     */
    size_t send(const output::BMSSnapshot& data);

    /**
     * Add a new log sink
     * @param sink_type Type of sink (serial, udp, tcp, mqtt, http, etc.)
     * @param config Configuration parameters for the sink
     * @return true if sink added successfully
     */
    bool addSink(const std::string& sink_type, const std::string& config);

    /**
     * Remove a log sink by type
     * @param sink_type Type of sink to remove
     * @return true if sink removed
     */
    bool removeSink(const std::string& sink_type);

    /**
     * Get list of active sinks
     * @return vector of sink names
     */
    std::vector<std::string> getActiveSinks() const;

    /**
     * Get status of specific sink
     * @param sink_type Sink type to check
     * @return true if sink is active
     */
    bool isSinkActive(const std::string& sink_type) const;

    /**
     * Get last error for a specific sink
     * @param sink_type Sink type to check
     * @return error string or empty if OK
     */
    std::string getSinkError(const std::string& sink_type) const;

    /**
     * Shutdown all sinks and cleanup
     */
    void shutdown();

    /**
     * Global stats
     */
    struct Stats {
        size_t total_messages_sent = 0;
        size_t total_bytes_sent = 0;
        size_t sinks_active = 0;
        size_t sinks_failed = 0;
        uint32_t uptime_ms = 0;
    };

    /**
     * Get statistics across all sinks
     */
    Stats getStats() const;

    // Sink creation function type
    using SinkCreator = std::function<LogSinkPtr(const std::string& config)>;

    /**
     * Register a new sink type factory
     * @param sink_type Type name
     * @param creator Factory function
     */
    void registerSink(const std::string& sink_type, SinkCreator creator);

private:
    // Private constructor for singleton
    LogManager();
    ~LogManager() = default;

    // Sink registry
    std::map<std::string, SinkCreator> sink_factories_;

    // Active sinks
    std::map<std::string, std::unique_ptr<LogSink>> active_sinks_;

    // Configuration parser
    struct SinkConfig {
        std::string type;
        std::string config;
        bool enabled = true;
    };

    std::vector<SinkConfig> parseConfiguration(const std::string& config);

    // Internal helper methods
    LogSinkPtr createSink(const std::string& sink_type, const std::string& config);

    // Default factory registrations
    void registerDefaultSinks();

    // Set last error helper
    void setLastError(const std::string& err);

    // Get last error helper
    std::string getLastError() const;

private:
    std::string last_error_;
};

/**
 * Convenience macros for logging
 * Example: LOG_SEND(data) - sends BMS data to all configured sinks
 */
#define LOG_INIT(config) logging::LogManager::getInstance().init(config)
#define LOG_SEND(data) logging::LogManager::getInstance().send(data)
#define LOG_SHUTDOWN() logging::LogManager::getInstance().shutdown()

} // namespace logging

#endif // LOG_MANAGER_H
