#include "log_serializers.h"
#include <string>
#include <stdlib.h>
#include <stdio.h>
#include <sstream>
#include <map>
#include <iomanip>

namespace logging {

std::map<std::string, SerializationFormat> _formatMap = {
    {"json", SerializationFormat::JSON},
    {"csv", SerializationFormat::CSV},
    {"xml", SerializationFormat::XML},
    {"binary", SerializationFormat::BINARY},
    {"human", SerializationFormat::HUMAN},
    {"kv", SerializationFormat::KEY_VALUE}
};

const char* formatToString(SerializationFormat format) {
    switch (format) {
        case SerializationFormat::JSON: return "json";
        case SerializationFormat::CSV: return "csv";
        case SerializationFormat::XML: return "xml";
        case SerializationFormat::BINARY: return "binary";
        case SerializationFormat::HUMAN: return "human";
        case SerializationFormat::KEY_VALUE: return "kv";
        default: return "unknown";
    }
}

SerializationFormat stringToFormat(const std::string& format_str) {
    auto it = _formatMap.find(format_str);
    return (it != _formatMap.end()) ? it->second : SerializationFormat::JSON;
}

/**
 * JSON serializer implementation
 */
class JSONSerializer : public BMSSerializer {
public:
    JSONSerializer() = default;
    ~JSONSerializer() override = default;

    bool serialize(const output::BMSSnapshot& data, std::string& result) override {
        std::ostringstream json;
        json << std::fixed << std::setprecision(3);

        json << "{\n";
        json << "  \"timestamp\": " << data.now_time_us << ",\n";
        json << "  \"elapsed_seconds\": " << data.elapsed_sec << ",\n";
        json << "  \"elapsed_hms\": \"" << data.hours << ":"
                << data.minutes << ":" << data.seconds << "\",\n";
        json << "  \"total_energy_wh\": " << data.total_energy_wh << ",\n";

        json << "  \"pack\": {\n";
        json << "    \"voltage_v\": " << data.pack_voltage_v << ",\n";
        json << "    \"current_a\": " << data.pack_current_a << ",\n";
        json << "    \"soc_pct\": " << data.soc_pct << ",\n";
        json << "    \"power_w\": " << data.power_w << ",\n";
        json << "    \"full_capacity_ah\": " << data.full_capacity_ah << "\n";
        json << "  },\n";

        json << "  \"stats\": {\n";
        json << "    \"peak_current_a\": " << data.peak_current_a << ",\n";
        json << "    \"peak_power_w\": " << data.peak_power_w << "\n";
        json << "  },\n";

        json << "  \"cells\": {\n";
        json << "    \"count\": " << data.cell_count << ",\n";
        json << "    \"min_voltage_v\": " << data.min_cell_voltage_v << ",\n";
        json << "    \"max_voltage_v\": " << data.max_cell_voltage_v << ",\n";
        json << "    \"min_cell\": " << data.min_cell_num << ",\n";
        json << "    \"max_cell\": " << data.max_cell_num << ",\n";
        json << "    \"voltage_delta_v\": " << data.cell_voltage_delta_v << ",\n";
        json << "    \"values\": [";

        for (int i = 0; i < data.cell_count; ++i) {
            if (i > 0) json << ",";
            json << data.cell_v[i];
        }
        json << "]\n  },\n";

        json << "  \"temperatures\": {\n";
        json << "    \"count\": " << data.temp_count << ",\n";
        json << "    \"min_c\": " << data.min_temp_c << ",\n";
        json << "    \"max_c\": " << data.max_temp_c << ",\n";
        json << "    \"values\": [";

        for (int i = 0; i < data.temp_count; ++i) {
            if (i > 0) json << ",";
            json << data.temp_c[i];
        }
        json << "]\n  },\n";

        json << "  \"status\": {\n";
        json << "    \"charging_enabled\": " << (data.charging_enabled ? "true" : "false") << ",\n";
        json << "    \"discharging_enabled\": " << (data.discharging_enabled ? "true" : "false") << "\n";
        json << "  }\n";
        json << "}\n";

        result = json.str();
        return true;
    }

    SerializationFormat getFormat() const override {
        return SerializationFormat::JSON;
    }

    std::string getContentType() const override {
        return "application/json";
    }

    std::string getHeader() const override {
        return ""; // JSON doesn't need headers
    }

    bool hasHeader() const override {
        return false; // JSON format doesn't use headers
    }

    bool supportsBatching() const override { return true; }
};

/**
 * CSV serializer implementation
 */
class CSVSerializer : public BMSSerializer {
private:
    bool header_printed_ = false;
    output::OutputConfig cfg_;

public:
    CSVSerializer(int max_cells = output::DEFAULT_MAX_CSV_CELLS,
                 int max_temps = output::DEFAULT_MAX_CSV_TEMPS) {
        cfg_.header_cells = max_cells;
        cfg_.header_temps = max_temps;
        cfg_.csv_print_header_once = true;
        cfg_.format = output::OutputFormat::CSV;
    }

    bool serialize(const output::BMSSnapshot& data, std::string& result) override {
        // For CSV, we'll reuse the existing implementation
        // This is a simplified version - in practice, you might want to use the existing
        // CSV formatting directly or create a more efficient implementation
        result.clear();

        char buffer[1024];
        int len = snprintf(buffer, sizeof(buffer),
            "%lld,%u,%02u:%02u:%02u,%.3f,%.2f,%.2f,%.1f,%.2f,%.2f,%.2f,%.2f,%d,%.3f,%d,%.3f,%d,%.3f,%d,%.1f,%.1f,%d,%d",
            (long long)data.real_timestamp,
            data.elapsed_sec, data.hours, data.minutes, data.seconds,
            data.total_energy_wh,
            data.pack_voltage_v, data.pack_current_a, data.soc_pct, data.power_w,
            data.full_capacity_ah, data.peak_current_a, data.peak_power_w, data.cell_count,
            data.min_cell_voltage_v, data.min_cell_num, data.max_cell_voltage_v,
            data.max_cell_num, data.cell_voltage_delta_v, data.temp_count,
            data.min_temp_c, data.max_temp_c, data.charging_enabled ? 1 : 0,
            data.discharging_enabled ? 1 : 0);

        result += std::string(buffer, len);

        int cells = (data.cell_count < cfg_.header_cells) ? data.cell_count : cfg_.header_cells;
        for (int i = 0; i < cells; ++i) {
            len = snprintf(buffer, sizeof(buffer), ",%.3f", data.cell_v[i]);
            result += std::string(buffer, len);
        }

        int temps = (data.temp_count < cfg_.header_temps) ? data.temp_count : cfg_.header_temps;
        for (int i = 0; i < temps; ++i) {
            len = snprintf(buffer, sizeof(buffer), ",%.1f", data.temp_c[i]);
            result += std::string(buffer, len);
        }

        return true;
    }

    SerializationFormat getFormat() const override {
        return SerializationFormat::CSV;
    }

    std::string getContentType() const override {
        return "text/csv";
    }

    std::string getHeader() const override {
        std::string header = "timestamp,elapsed_sec,hours:minutes:seconds,total_energy_wh,pack_voltage_v,pack_current_a,soc_pct,power_w,full_capacity_ah,peak_current_a,peak_power_w,cell_count,min_cell_voltage_v,min_cell_num,max_cell_voltage_v,max_cell_num,cell_voltage_delta_v,temp_count,min_temp_c,max_temp_c,charging_enabled,discharging_enabled";
        
        // Add cell voltage headers
        for (int i = 0; i < cfg_.header_cells; ++i) {
            header += ",cell_v_" + std::to_string(i + 1);
        }
        
        // Add temperature headers
        for (int i = 0; i < cfg_.header_temps; ++i) {
            header += ",temp_c_" + std::to_string(i + 1);
        }
        
        header += "\n";
        return header;
    }

    bool setOptions(const std::string& options) override {
        // Parse options like "max_cells=16,max_temps=8"
        // Basic parsing - in real implementation, parse JSON
        return true;
    }
};

// Factory method implementations
std::unique_ptr<BMSSerializer> BMSSerializer::createSerializer(SerializationFormat format) {
    switch (format) {
        case SerializationFormat::JSON: return std::make_unique<JSONSerializer>();
        case SerializationFormat::CSV: return std::make_unique<CSVSerializer>();
        // TODO: Implement other formats
        default: return nullptr;
    }
}

std::unique_ptr<BMSSerializer> BMSSerializer::createSerializer(const std::string& format_str) {
    return createSerializer(stringToFormat(format_str));
}

} // namespace logging