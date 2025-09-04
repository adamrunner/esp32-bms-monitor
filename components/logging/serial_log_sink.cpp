#include "serial_log_sink.h"
#include <tuple>
#include <iostream>
#include <iomanip>
#include <limits>

// ESP-IDF includes
#include <esp_log.h>
#include <driver/uart.h>
#include <cJSON.h>

using namespace logging;

// Default constructor
SerialLogSink::SerialLogSink() :
    serializer_(nullptr),
    initialized_(false),
    config_print_header_(true),
    printed_header_(false)
{
    setLastError("");
}

// Destructor
SerialLogSink::~SerialLogSink() {
    shutdown();
}

bool SerialLogSink::init(const std::string& config) {
    // Parse configuration
    if (!parseConfig(config)) {
        setLastError("Failed to parse configuration");
        return false;
    }

    // Create appropriate serializer
    if (config_.format == "csv") {
        // For now, we'll use the base BMSSerializer class for CSV format
        serializer_ = logging::BMSSerializer::createSerializer("csv");
        config_.print_header = true;  // Always print CSV header when using CSV format
    }
    else if (config_.format == "json") {
        serializer_ = logging::BMSSerializer::createSerializer("json");
    }
    else {
        // "human" format - we'll use the base BMSSerializer for CSV format
        // but format the output for human readability
        serializer_ = logging::BMSSerializer::createSerializer("csv");
    }

    if (!serializer_) {
        setLastError("Failed to create serializer");
        return false;
    }

    // Initialize UART for debug output (if not already initialized)
    // Note: In ESP-IDF, this is typically done already

    initialized_ = true;
    return true;
}

bool SerialLogSink::send(const output::BMSSnapshot& data) {
    if (!initialized_ || !isReady()) {
        return false;
    }

    std::string serialized;
    if (!serializer_->serialize(data, serialized)) {
        setLastError("Failed to serialize data");
        return false;
    }

    // Print based on format
    if (config_.format == "human") {
        // Human-friendly format - manually format
        std::cout << "\n=== BMS Monitor Data ===\n";
        std::cout << "Elapsed Time: " << std::setfill('0') << std::setw(2) << data.hours << ":"
                  << std::setw(2) << data.minutes << ":" << std::setw(2) << data.seconds << "\n";
        std::cout << "Total Energy: " << data.total_energy_wh << " Wh\n";
        std::cout << "Pack Voltage: " << data.pack_voltage_v << " V\n";
        std::cout << "Pack Current: " << data.pack_current_a << " A\n";
        std::cout << "State of Charge: " << data.soc_pct << " %\n";
        std::cout << "Power: " << data.power_w << " W\n";
        std::cout << "Peak Current: " << data.peak_current_a << " A\n";
        std::cout << "Peak Power: " << data.peak_power_w << " W\n";
        std::cout << "Cell Count: " << data.cell_count << "\n";
        std::cout << "Voltage Range: " << data.min_cell_voltage_v << " V - " << data.max_cell_voltage_v << " V\n";

        std::cout << "Individual Cell Voltages:\n";
        for (int i = 0; i < data.cell_count; ++i) {
            std::cout << "  Cell " << (i+1) << ": " << data.cell_v[i] << " V\n";
        }

        std::cout << "Temperatures:\n";
        for (int i = 0; i < data.temp_count; ++i) {
            std::cout << "  Temp " << (i+1) << ": " << data.temp_c[i] << " °C\n";
        }

        std::cout.flush();
    }
    else {
<<<<<<< HEAD
        // Print header if the serializer supports it
        if (config_.print_header && !printed_header_ && serializer_->hasHeader()) {
            std::string header = serializer_->getHeader();
            if (!header.empty()) {
                std::cout << header;
                printed_header_ = true;
            }
=======
        // For CSV format, optionally print header first
        if (config_.format == "csv" && config_.print_header && !printed_header_) {
            // Print CSV header
            std::cout << "timestamp,elapsed_sec,hours:minutes:seconds,total_energy_wh,pack_voltage_v,pack_current_a,soc_pct,power_w,full_capacity_ah,peak_current_a,peak_power_w,cell_count,min_cell_voltage_v,min_cell_num,max_cell_voltage_v,max_cell_num,cell_voltage_delta_v,temp_count,min_temp_c,max_temp_c,charging_enabled,discharging_enabled";

            // Add cell voltage headers
            for (int i = 0; i < config_.max_cells; ++i) {
                std::cout << ",cell_v_" << (i+1);
            }

            // Add temperature headers
            for (int i = 0; i < config_.max_temps; ++i) {
                std::cout << ",temp_c_" << (i+1);
            }

            std::cout << "\n";
            printed_header_ = true;
>>>>>>> esp-idf
        }

        // JSON or CSV format - send as-is
        std::cout << serialized;
        if (config_.format == "csv" || config_.format == "json") {
            std::cout << "\n";
        }
        std::cout << std::flush;
    }

    return true;
}

void SerialLogSink::shutdown() {
    serializer_.reset();
    initialized_ = false;
}

const char* SerialLogSink::getName() const {
    return "serial";
}

bool SerialLogSink::isReady() const {
    return initialized_;
}

bool SerialLogSink::parseConfig(const std::string& config_str) {
    // Parse JSON configuration
    cJSON *json = cJSON_Parse(config_str.c_str());
    if (json) {
        // New JSON format parsing
        cJSON *format_item = cJSON_GetObjectItemCaseSensitive(json, "format");
        if (cJSON_IsString(format_item)) {
            config_.format = std::string(format_item->valuestring);
        }

        cJSON *print_header_item = cJSON_GetObjectItemCaseSensitive(json, "print_header");
        if (cJSON_IsBool(print_header_item)) {
            config_.print_header = cJSON_IsTrue(print_header_item);
        }

        cJSON *max_cells_item = cJSON_GetObjectItemCaseSensitive(json, "max_cells");
        if (cJSON_IsNumber(max_cells_item)) {
            config_.max_cells = max_cells_item->valueint;
        }

        cJSON *max_temps_item = cJSON_GetObjectItemCaseSensitive(json, "max_temps");
        if (cJSON_IsNumber(max_temps_item)) {
            config_.max_temps = max_temps_item->valueint;
        }

        cJSON_Delete(json);
        return true;
    } else {
        // Fallback to old key=value parser for compatibility
        // Format: "format=csv,print_header=true,max_cells=16,max_temps=8"
        std::string config = config_str;
        config += ","; // Add sentinel

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

            // Strip quotes if present
            if (!value.empty() && value.front() == '"' && value.back() == '"') {
                value = value.substr(1, value.length()-2);
            }

            if (key == "format") config_.format = value;
            else if (key == "print_header") config_.print_header = (value == "true");
            else if (key == "max_cells") config_.max_cells = atoi(value.c_str());
            else if (key == "max_temps") config_.max_temps = atoi(value.c_str());

            start = next_comma + 1;
            pos = config.find('=', start);
            if (next_comma+1 >= config.length()) break;
        }

        return true;
    }
}
