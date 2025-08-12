#include <cstdio>
#include <cmath>
#include "logging.h"

namespace logging
{

static bool g_csv_header_printed = false;

static inline int clamp_cells(int requested)
{
    return requested > DEFAULT_MAX_CSV_CELLS ? DEFAULT_MAX_CSV_CELLS : (requested < 0 ? 0 : requested);
}

static inline int clamp_temps(int requested)
{
    return requested > DEFAULT_MAX_CSV_TEMPS ? DEFAULT_MAX_CSV_TEMPS : (requested < 0 ? 0 : requested);
}

static void print_human(const MeasurementSnapshot& s, const LogConfig& cfg)
{
    (void)cfg; // unused for now

    std::printf("\n=== BMS Monitor Data ===\n");
    std::printf("Elapsed Time: %02u:%02u:%02u (hh:mm:ss)\n", s.hours, s.minutes, s.seconds);
    std::printf("Total Energy: %.3f Wh\n", s.total_energy_wh);
    std::printf("Pack Voltage: %.2f V\n", s.pack_voltage_v);
    std::printf("Pack Current: %.2f A\n", s.pack_current_a);
    std::printf("State of Charge: %.1f%%\n", s.soc_pct);
    std::printf("Power: %.2f W\n", s.power_w);
    if (s.full_capacity_ah > 0.0f) {
        std::printf("Full Capacity: %.2f Ah\n", s.full_capacity_ah);
    }
    std::printf("Peak Current: %.2f A\n", s.peak_current_a);
    std::printf("Peak Power: %.2f W\n", s.peak_power_w);
    std::printf("Cell Count: %d\n", s.cell_count);
    std::printf("Cell Voltage Range: %.3f V (Cell %d) - %.3f V (Cell %d)\n",
                s.min_cell_voltage_v, s.min_cell_num, s.max_cell_voltage_v, s.max_cell_num);
    std::printf("Cell Voltage Delta: %.3f V\n", s.cell_voltage_delta_v);
    std::printf("Temperature Count: %d\n", s.temp_count);
    std::printf("Temperature Range: %.1f°C - %.1f°C\n", s.min_temp_c, s.max_temp_c);
    std::printf("Charging Enabled: %s\n", s.charging_enabled ? "YES" : "NO");
    std::printf("Discharging Enabled: %s\n", s.discharging_enabled ? "YES" : "NO");
    std::printf("========================\n");

    std::printf("Individual Cell Voltages:\n");
    int cells_to_print = clamp_cells(s.cell_count);
    for (int i = 0; i < cells_to_print; ++i) {
        std::printf("  Cell %d: %.3f V\n", i + 1, s.cell_v[static_cast<size_t>(i)]);
    }

    std::printf("Individual Temperatures:\n");
    int temps_to_print = clamp_temps(s.temp_count);
    for (int i = 0; i < temps_to_print; ++i) {
        std::printf("  Temp %d: %.1f°C\n", i + 1, s.temp_c[static_cast<size_t>(i)]);
    }
}

static void print_csv_header(const LogConfig& cfg)
{
    std::printf("elapsed_seconds,elapsed_hms,total_energy_wh,pack_voltage_v,pack_current_a,state_of_charge_pct,");
    std::printf("power_w,full_capacity_ah,peak_current_a,peak_power_w,cell_count,");
    std::printf("min_cell_voltage_v,min_cell_num,max_cell_voltage_v,max_cell_num,cell_voltage_delta_v,");
    std::printf("temp_count,min_temp_c,max_temp_c,charging_enabled,discharging_enabled");

    int cells = clamp_cells(cfg.header_cells);
    for (int i = 1; i <= cells; ++i) {
        std::printf(",cells_v_%d", i);
    }

    int temps = clamp_temps(cfg.header_temps);
    for (int i = 1; i <= temps; ++i) {
        std::printf(",temps_c_%d", i);
    }

    std::printf("\n");
}

static void print_csv_row(const MeasurementSnapshot& s, const LogConfig& cfg)
{
    // Scalars
    std::printf("%u,%02u:%02u:%02u,%.3f,%.2f,%.2f,%.1f,%.2f,",
                s.elapsed_sec, s.hours, s.minutes, s.seconds,
                s.total_energy_wh,
                s.pack_voltage_v, s.pack_current_a, s.soc_pct, s.power_w);

    if (s.full_capacity_ah > 0.0f) {
        std::printf("%.2f,", s.full_capacity_ah);
    } else {
        std::printf(",");
    }

    std::printf("%.2f,%.2f,%d,",
                s.peak_current_a,
                s.peak_power_w,
                s.cell_count);

    std::printf("%.3f,%d,%.3f,%d,%.3f,%d,",
                s.min_cell_voltage_v,
                s.min_cell_num,
                s.max_cell_voltage_v,
                s.max_cell_num,
                s.cell_voltage_delta_v,
                s.temp_count);

    std::printf("%.1f,%.1f,%d,%d",
                s.min_temp_c,
                s.max_temp_c,
                s.charging_enabled ? 1 : 0,
                s.discharging_enabled ? 1 : 0);

    // Cells
    int cells = clamp_cells(cfg.header_cells);
    for (int i = 0; i < cells; ++i) {
        std::printf(",");
        if (i < s.cell_count) {
            std::printf("%.3f", s.cell_v[static_cast<size_t>(i)]);
        }
    }

    // Temps
    int temps = clamp_temps(cfg.header_temps);
    for (int i = 0; i < temps; ++i) {
        std::printf(",");
        if (i < s.temp_count) {
            std::printf("%.1f", s.temp_c[static_cast<size_t>(i)]);
        }
    }

    std::printf("\n");
}

void log_emit(const MeasurementSnapshot& s, const LogConfig& cfg)
{
    if (cfg.format == LogFormat::CSV) {
        if (cfg.csv_print_header_once && !g_csv_header_printed) {
            print_csv_header(cfg);
            g_csv_header_printed = true;
        }
        print_csv_row(s, cfg);
    } else {
        print_human(s, cfg);
    }
}

} // namespace logging
