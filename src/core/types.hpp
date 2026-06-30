#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

struct CpuTicks {
    uint64_t user_ticks = 0;
    uint64_t system_ticks = 0;
    uint64_t idle_ticks = 0;
    uint64_t nice_ticks = 0;
};

struct PowerMetrics {
    std::optional<double> cpu_avg_temp_celsius;
    std::optional<double> cpu_hot_temp_celsius;
    std::optional<double> cpu_power_watts;
    std::optional<double> efficiency_freq_mhz;
    std::optional<double> performance_freq_mhz;
    std::optional<double> gpu_usage_pct;
    std::optional<double> gpu_avg_temp_celsius;
    std::optional<double> gpu_hot_temp_celsius;
    std::optional<double> gpu_freq_mhz;
    std::optional<double> gpu_power_watts;
    std::optional<double> system_power_watts;
    std::string error;
};

struct BatteryMetrics {
    std::optional<double> capacity_pct;
    std::optional<double> input_power_watts;
    std::optional<double> battery_power_watts;
    std::optional<double> cycle_count;
    bool is_charging = false;
};

struct ClusterReading {
    std::optional<double> frequency_mhz;
    std::optional<double> active_residency_pct;
    std::optional<double> online_pct;
};

struct MemoryMetrics {
    double used_pct = 0.0;
    uint64_t used_bytes = 0;
    uint64_t total_bytes = 0;
    uint64_t available_bytes = 0;
    uint64_t cached_bytes = 0;
    uint64_t free_bytes = 0;
    uint64_t swap_used_bytes = 0;
    uint64_t swap_total_bytes = 0;
};

struct TemperatureGroups {
    std::vector<double> cpu_temps;
    std::vector<double> gpu_temps;
};

struct SystemSample {
    std::vector<double> core_usage_pct;
    double cpu_average_pct = 0.0;
    PowerMetrics power;
    BatteryMetrics battery;
    MemoryMetrics memory;
};
