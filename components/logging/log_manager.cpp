#include "log_manager.h"
#include <time.h>
#include <esp_log.h>
#include <cJSON.h>

// Include actual sink implementations
#ifdef INCLUDE_SERIAL_SINK
#include "serial_log_sink.h"
#endif
#ifdef INCLUDE_UDP_SINK
#include "udp_log_sink.h"
#endif
#ifdef INCLUDE_TCP_SINK
#include "tcp_log_sink.h"
#endif
#ifdef INCLUDE_MQTT_SINK
#include "mqtt_log_sink.h"
#endif
#ifdef INCLUDE_SDCARD_SINK
#include "sd_card_log_sink.h"
#endif

namespace logging {

// Static initialization
LogManager& LogManager::getInstance() {
    static LogManager instance;
    return instance;
}

LogManager::LogManager() {
    registerDefaultSinks();
}

void LogManager::registerDefaultSinks() {
    #ifdef INCLUDE_SERIAL_SINK
    registerSink("serial", [] (const std::string& config) {
        return std::make_unique<SerialLogSink>();
    });
    #endif

    #ifdef INCLUDE_UDP_SINK
    registerSink("udp", [] (const std::string& config) {
        return std::make_unique<UDPLogSink>();
    });
    #endif

    #ifdef INCLUDE_TCP_SINK
    registerSink("tcp", [] (const std::string& config) {
        return std::make_unique<TCPLogSink>();
    });
    #endif

    #ifdef INCLUDE_MQTT_SINK
    registerSink("mqtt", [] (const std::string& config) {
        return std::make_unique<MQTTLogSink>();
    });
    #endif

    #ifdef INCLUDE_SDCARD_SINK
    registerSink("sdcard", [] (const std::string& config) {
        return std::make_unique<SDCardLogSink>();
    });
    #endif
}

bool LogManager::init(const std::string& config) {
    // Parse configuration
    auto sink_configs = parseConfiguration(config);

    size_t successful = 0;
    for (const auto& sink_config : sink_configs) {
        if (!sink_config.enabled) continue;

        if (addSink(sink_config.type, sink_config.config)) {
            successful++;
        } else {
            ESP_LOGW("LogManager", "Failed to add sink %s: %s",
                    sink_config.type.c_str(), getSinkError(sink_config.type).c_str());
        }
    }

    return successful > 0;
}

std::vector<LogManager::SinkConfig> LogManager::parseConfiguration(const std::string& config) {
    std::vector<SinkConfig> result;

    // Parse JSON configuration
    cJSON *json = cJSON_Parse(config.c_str());
    if (!json) {
        ESP_LOGE("LogManager", "Failed to parse JSON config: %s", config.c_str());
        return result;
    }

    // Check if it's the new format with "sinks" array
    cJSON *sinks_array = cJSON_GetObjectItemCaseSensitive(json, "sinks");
    if (cJSON_IsArray(sinks_array)) {
        // New format: {"sinks": [{"type": "serial", "config": {...}}, ...]}
        cJSON *sink_item = NULL;
        cJSON_ArrayForEach(sink_item, sinks_array) {
            if (cJSON_IsObject(sink_item)) {
                SinkConfig sc;

                // Get type
                cJSON *type_item = cJSON_GetObjectItemCaseSensitive(sink_item, "type");
                if (cJSON_IsString(type_item)) {
                    sc.type = std::string(type_item->valuestring);
                } else {
                    continue; // Skip if no valid type
                }

                // Get enabled status (default to true)
                cJSON *enabled_item = cJSON_GetObjectItemCaseSensitive(sink_item, "enabled");
                sc.enabled = cJSON_IsBool(enabled_item) ? cJSON_IsTrue(enabled_item) : true;

                // Get config
                cJSON *config_item = cJSON_GetObjectItemCaseSensitive(sink_item, "config");
                if (cJSON_IsObject(config_item)) {
                    // Convert config object to string
                    char *config_str = cJSON_PrintUnformatted(config_item);
                    if (config_str) {
                        sc.config = std::string(config_str);
                        cJSON_free(config_str);
                    }
                } else if (cJSON_IsString(config_item)) {
                    sc.config = std::string(config_item->valuestring);
                } else {
                    // Empty config
                    sc.config = "{}";
                }

                result.push_back(sc);
            }
        }
    } else {
        // Fallback to old format for compatibility
        // Simple config format parser
        // Expected format: "type:config,type:config"
        // Example: "serial:format=csv;print_header=true,udp:ip=192.168.1.100;port=3330"

        size_t start = 0;
        size_t comma = config.find(',', start);

        // If no comma found, treat the whole string as a single sink config
        if (comma == std::string::npos) {
            size_t colon = config.find(':');
            if (colon != std::string::npos) {
                SinkConfig sc;
                sc.type = config.substr(0, colon);
                sc.config = config.substr(colon + 1);
                sc.enabled = true;
                result.push_back(sc);
            }
            cJSON_Delete(json);
            return result;
        }

        // Multiple sink configs separated by commas
        while (start < config.length()) {
            size_t colon = config.find(':', start);
            if (colon == std::string::npos) break;

            size_t next_comma = config.find(',', colon);
            if (next_comma == std::string::npos) next_comma = config.length();

            SinkConfig sc;
            sc.type = config.substr(start, colon - start);
            sc.config = config.substr(colon + 1, next_comma - colon - 1);
            sc.enabled = true;

            result.push_back(sc);
            start = next_comma + 1;
        }
    }

    cJSON_Delete(json);
    return result;
}

size_t LogManager::send(const output::BMSSnapshot& data) {
    size_t successful = 0;
    for (const auto& sink_pair : active_sinks_) {
        if (sink_pair.second->send(data)) {
            successful++;
        }
    }

    return successful;
}

bool LogManager::addSink(const std::string& sink_type, const std::string& config) {
    auto it = sink_factories_.find(sink_type);
    if (it == sink_factories_.end()) {
        setLastError("Unknown sink type: " + sink_type);
        return false;
    }

    LogSinkPtr new_sink = it->second(config);
    if (!new_sink) {
        setLastError("Factory failed to create sink: " + sink_type);
        return false;
    }

    if (!new_sink->init(config)) {
        setLastError(sink_type + " initialization failed: " + new_sink->getLastError());
        return false;
    }

    // Remove any existing sink of this type
    removeSink(sink_type);

    active_sinks_.emplace(sink_type, std::move(new_sink));
    return true;
}

bool LogManager::removeSink(const std::string& sink_type) {
    auto it = active_sinks_.find(sink_type);
    if (it == active_sinks_.end()) {
        return false;
    }

    it->second->shutdown();
    active_sinks_.erase(it);
    return true;
}

std::vector<std::string> LogManager::getActiveSinks() const {
    std::vector<std::string> result;
    for (const auto& sink_pair : active_sinks_) {
        result.push_back(sink_pair.first);
    }
    return result;
}

bool LogManager::isSinkActive(const std::string& sink_type) const {
    return active_sinks_.find(sink_type) != active_sinks_.end();
}

std::string LogManager::getSinkError(const std::string& sink_type) const {
    auto it = active_sinks_.find(sink_type);
    if (it == active_sinks_.end()) {
        return "Sink not active";
    }
    return it->second->getLastError();
}

LogManager::Stats LogManager::getStats() const {
    Stats stats;
    stats.sinks_active = active_sinks_.size();
    stats.sinks_failed = 0;

    // TODO: Implement proper stats collection
    // For now, just return basic counts

    return stats;
}

void LogManager::registerSink(const std::string& sink_type, SinkCreator creator) {
    sink_factories_[sink_type] = creator;
}

void LogManager::shutdown() {
    for (auto& sink_pair : active_sinks_) {
        sink_pair.second->shutdown();
    }
    active_sinks_.clear();
}

// Set last error helper
void LogManager::setLastError(const std::string& err) {
    last_error_ = err;
}

// Get last error helper
std::string LogManager::getLastError() const {
    return last_error_;
}

} // namespace logging