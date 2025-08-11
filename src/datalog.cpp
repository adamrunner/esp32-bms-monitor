#include <Arduino.h>
#include <cmath>
#include "datalog.h"

namespace datalog
{

static bool g_csv_header_printed = false;

static inline int clamp_cells(int requested)
{
    return requested > DEFAULT_MAX_CELLS ? DEFAULT_MAX_CELLS : (requested < 0 ? 0 : requested);
}

static inline int clamp_temps(int requested)
{
    return requested > DEFAULT_MAX_TEMPS ? DEFAULT_MAX_TEMPS : (requested < 0 ? 0 : requested);
}

static void print_human(const Snapshot& s, const Config& cfg)
{
    (void)cfg; // unused for now

    Serial.println("\n=== BMS Monitor Data ===");
    Serial.printf("Elapsed Time: %02u:%02u:%02u (hh:mm:ss)\n", s.hours, s.minutes, s.seconds);
    Serial.printf("Total Energy: %.3f Wh\n", s.total_energy_wh);
    Serial.printf("Pack Voltage: %.2f V\n", s.pack_voltage_v);
    Serial.printf("Pack Current: %.2f A\n", s.pack_current_a);
    Serial.printf("State of Charge: %.1f%%\n", s.soc_pct);
    Serial.printf("Power: %.2f W\n", s.power_w);
    if (s.full_capacity_ah > 0.0f) {
        Serial.printf("Full Capacity: %.2f Ah\n", s.full_capacity_ah);
    }
    Serial.printf("Peak Current: %.2f A\n", s.peak_current_a);
    Serial.printf("Peak Power: %.2f W\n", s.peak_power_w);
    Serial.printf("Cell Count: %d\n", s.cell_count);
    Serial.printf("Cell Voltage Range: %.3f V (Cell %d) - %.3f V (Cell %d)\n",
                s.min_cell_voltage_v, s.min_cell_num, s.max_cell_voltage_v, s.max_cell_num);
    Serial.printf("Cell Voltage Delta: %.3f V\n", s.cell_voltage_delta_v);
    Serial.printf("Temperature Count: %d\n", s.temp_count);
    Serial.printf("Temperature Range: %.1f°C - %.1f°C\n", s.min_temp_c, s.max_temp_c);
    Serial.printf("Charging Enabled: %s\n", s.charging_enabled ? "YES" : "NO");
    Serial.printf("Discharging Enabled: %s\n", s.discharging_enabled ? "YES" : "NO");
    Serial.printf("========================\n");

    Serial.printf("Individual Cell Voltages:\n");
    int cells_to_print = clamp_cells(s.cell_count);
    for (int i = 0; i < cells_to_print; ++i) {
        Serial.printf("  Cell %d: %.3f V\n", i + 1, s.cell_v[static_cast<size_t>(i)]);
    }

    Serial.printf("Individual Temperatures:\n");
    int temps_to_print = clamp_temps(s.temp_count);
    for (int i = 0; i < temps_to_print; ++i) {
        Serial.printf("  Temp %d: %.1f°C\n", i + 1, s.temp_c[static_cast<size_t>(i)]);
    }
}

static void print_csv_header(const Config& cfg)
{
    Serial.printf("elapsed_seconds,elapsed_hms,total_energy_wh,pack_voltage_v,pack_current_a,state_of_charge_pct,");
    Serial.printf("power_w,full_capacity_ah,peak_current_a,peak_power_w,cell_count,");
    Serial.printf("min_cell_voltage_v,min_cell_num,max_cell_voltage_v,max_cell_num,cell_voltage_delta_v,");
    Serial.printf("temp_count,min_temp_c,max_temp_c,charging_enabled,discharging_enabled");

    int cells = clamp_cells(cfg.header_cells);
    for (int i = 1; i <= cells; ++i) {
        Serial.printf(",cells_v_%d", i);
    }

    int temps = clamp_temps(cfg.header_temps);
    for (int i = 1; i <= temps; ++i) {
        Serial.printf(",temps_c_%d", i);
    }

    Serial.printf("\n");
}

void format_csv_row(String& out, const Snapshot& s, const Config& cfg)
{
    char buf[64];
    snprintf(buf, sizeof(buf), "%u,%02u:%02u:%02u,%.3f,%.2f,%.2f,%.1f,%.2f,",
             s.elapsed_sec, s.hours, s.minutes, s.seconds,
             s.total_energy_wh,
             s.pack_voltage_v, s.pack_current_a, s.soc_pct, s.power_w);
    out += buf;

    if (s.full_capacity_ah > 0.0f) {
        snprintf(buf, sizeof(buf), "%.2f,", s.full_capacity_ah);
        out += buf;
    } else {
        out += ",";
    }

    snprintf(buf, sizeof(buf), "%.2f,%.2f,%d,",
             s.peak_current_a,
             s.peak_power_w,
             s.cell_count);
    out += buf;

    snprintf(buf, sizeof(buf), "%.3f,%d,%.3f,%d,%.3f,%d,",
             s.min_cell_voltage_v,
             s.min_cell_num,
             s.max_cell_voltage_v,
             s.max_cell_num,
             s.cell_voltage_delta_v,
             s.temp_count);
    out += buf;

    snprintf(buf, sizeof(buf), "%.1f,%.1f,%d,%d",
             s.min_temp_c,
             s.max_temp_c,
             s.charging_enabled ? 1 : 0,
             s.discharging_enabled ? 1 : 0);
    out += buf;

    int cells = clamp_cells(cfg.header_cells);
    for (int i = 0; i < cells; ++i) {
        out += ",";
        if (i < s.cell_count) {
            snprintf(buf, sizeof(buf), "%.3f", s.cell_v[static_cast<size_t>(i)]);
            out += buf;
        }
    }

    int temps = clamp_temps(cfg.header_temps);
    for (int i = 0; i < temps; ++i) {
        out += ",";
        if (i < s.temp_count) {
            snprintf(buf, sizeof(buf), "%.1f", s.temp_c[static_cast<size_t>(i)]);
            out += buf;
        }
    }
}

void emit(const Snapshot& s, const Config& cfg)
{
    if (cfg.format == Format::CSV) {
        if (cfg.csv_print_header_once && !g_csv_header_printed) {
            print_csv_header(cfg);
            g_csv_header_printed = true;
        }
        String line;
        format_csv_row(line, s, cfg);
        Serial.println(line);
    } else {
        print_human(s, cfg);
    }
}

} // namespace datalog