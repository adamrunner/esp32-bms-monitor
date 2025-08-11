#ifndef LOGGING_H
#define LOGGING_H

#include <array>
#include <cstdint>

namespace logging
{

// Compile-time defaults
constexpr int DEFAULT_MAX_CSV_CELLS = 16;
constexpr int DEFAULT_MAX_CSV_TEMPS = 8;

enum class LogFormat
{
    Human = 0,
    CSV   = 1
};

struct LogConfig
{
#ifdef LOG_FORMAT_CSV
    LogFormat format { LogFormat::CSV };
#else
    LogFormat format { LogFormat::Human };
#endif
    bool csv_print_header_once { true };
    int header_cells { DEFAULT_MAX_CSV_CELLS };
    int header_temps { DEFAULT_MAX_CSV_TEMPS };
};

struct MeasurementSnapshot
{
    // Timing
    uint64_t start_time_us { 0 };
    uint64_t now_time_us { 0 };
    unsigned elapsed_sec { 0 };
    unsigned hours { 0 };
    unsigned minutes { 0 };
    unsigned seconds { 0 };

    // Energy
    double total_energy_wh { 0.0 };

    // Pack measurements
    float pack_voltage_v { 0.0f };
    float pack_current_a { 0.0f };
    float soc_pct { 0.0f };
    float power_w { 0.0f };
    float full_capacity_ah { 0.0f };

    // Peaks
    float peak_current_a { 0.0f };
    float peak_power_w { 0.0f };

    // Cell stats
    int cell_count { 0 };
    float min_cell_voltage_v { 0.0f };
    float max_cell_voltage_v { 0.0f };
    int min_cell_num { 0 }; // 1-based
    int max_cell_num { 0 }; // 1-based
    float cell_voltage_delta_v { 0.0f };

    // Temps
    int temp_count { 0 };
    float min_temp_c { 0.0f };
    float max_temp_c { 0.0f };

    // Status
    bool charging_enabled { false };
    bool discharging_enabled { false };

    // Arrays (fixed for CSV; human may still print up to these)
    std::array<float, DEFAULT_MAX_CSV_CELLS> cell_v{};
    std::array<float, DEFAULT_MAX_CSV_TEMPS> temp_c{};
};

// Emit one log record in the configured format to the current stdout/serial
void log_emit(const MeasurementSnapshot& s, const LogConfig& cfg);

// Utility: format a CSV row into a String (no newline)
void format_csv_row(String& out, const MeasurementSnapshot& s, const LogConfig& cfg);

} // namespace logging

#endif // LOGGING_H
