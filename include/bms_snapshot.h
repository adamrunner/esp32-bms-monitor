#pragma once

#include <stdint.h>
#include <time.h>

#ifdef __cplusplus
#include <array>

namespace output {

constexpr int DEFAULT_MAX_CSV_CELLS = 16;
constexpr int DEFAULT_MAX_CSV_TEMPS = 8;

enum class OutputFormat
{
    Human = 0,
    CSV   = 1
};

struct OutputConfig
{
#ifdef LOG_FORMAT_CSV
    OutputFormat format { OutputFormat::CSV };
#else
    OutputFormat format { OutputFormat::Human };
#endif
    bool csv_print_header_once { true };
    int header_cells { 16 };
    int header_temps { 8 };
};

struct BMSSnapshot
{
    uint64_t start_time_us { 0 };
    uint64_t now_time_us { 0 };
    unsigned elapsed_sec { 0 };
    unsigned hours { 0 };
    unsigned minutes { 0 };
    unsigned seconds { 0 };

    time_t real_timestamp { 0 };

    double total_energy_wh { 0.0 };

    float pack_voltage_v { 0.0f };
    float pack_current_a { 0.0f };
    float soc_pct { 0.0f };
    float power_w { 0.0f };
    float full_capacity_ah { 0.0f };

    float peak_current_a { 0.0f };
    float peak_power_w { 0.0f };

    int cell_count { 0 };
    float min_cell_voltage_v { 0.0f };
    float max_cell_voltage_v { 0.0f };
    int min_cell_num { 0 }; // 1-based
    int max_cell_num { 0 }; // 1-based
    float cell_voltage_delta_v { 0.0f };

    int temp_count { 0 };
    float min_temp_c { 0.0f };
    float max_temp_c { 0.0f };

    bool charging_enabled { false };
    bool discharging_enabled { false };

    std::array<float, DEFAULT_MAX_CSV_CELLS> cell_v{};
    std::array<float, DEFAULT_MAX_CSV_TEMPS> temp_c{};
};

} // namespace output

#else
#ifndef DEFAULT_MAX_CSV_CELLS
#define DEFAULT_MAX_CSV_CELLS 16
#endif
#ifndef DEFAULT_MAX_CSV_TEMPS
#define DEFAULT_MAX_CSV_TEMPS 8
#endif
#endif
