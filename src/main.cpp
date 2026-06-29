#include <ncurses.h>

#include <chrono>
#include <deque>
#include <thread>

#include "core/constants.hpp"
#include "core/types.hpp"
#include "io_report/io_report_power.hpp"
#include "io_report/io_report_freq.hpp"
#include "sensors/temperature.hpp"
#include "sensors/cpu.hpp"
#include "sensors/memory.hpp"
#include "sensors/battery.hpp"
#include "powermetrics/powermetrics.hpp"
#include "smc/smc.hpp"
#include "ui/ncurses.hpp"

static SystemSample collectSystemSample() {
    SystemSample sample;
    sample.core_usage_pct = readCpuUsage();
    if (!sample.core_usage_pct.empty()) {
        double sum = 0.0;
        for (double value : sample.core_usage_pct) sum += value;
        sample.cpu_average_pct = sum / static_cast<double>(sample.core_usage_pct.size());
    }
    sample.memory = readMemoryMetrics();
    sample.battery = readBatteryMetrics();

    sample.power = g_powermetrics_sampler.takeSnapshot();

    TemperatureGroups smc_temps = readSmcTemperatures();
    applyTemperatureGroup(sample.power, smc_temps.cpu_temps, false);
    applyTemperatureGroup(sample.power, smc_temps.gpu_temps, true);
    TemperatureGroups hid_temps = readHidTemperatures();
    applyTemperatureGroup(sample.power, hid_temps.cpu_temps, false);
    applyTemperatureGroup(sample.power, hid_temps.gpu_temps, true);

    if (!sample.power.system_power_watts) {
        if (sample.battery.input_power_watts && sample.battery.battery_power_watts) {
            sample.power.system_power_watts = *sample.battery.input_power_watts + *sample.battery.battery_power_watts;
        } else if (sample.battery.input_power_watts) {
            sample.power.system_power_watts = sample.battery.input_power_watts;
        } else if (sample.battery.battery_power_watts) {
            sample.power.system_power_watts = sample.battery.battery_power_watts;
        }
    }

    g_io_report_power.collect();
    if (g_io_report_power.cpu_power_watts && *g_io_report_power.cpu_power_watts > 0) sample.power.cpu_power_watts = g_io_report_power.cpu_power_watts;
    if (g_io_report_power.gpu_power_watts && *g_io_report_power.gpu_power_watts > 0) sample.power.gpu_power_watts = g_io_report_power.gpu_power_watts;

    g_io_report_freq.collect();
    if (!sample.power.efficiency_freq_mhz && g_io_report_freq.efficiency_freq_mhz && *g_io_report_freq.efficiency_freq_mhz > 0) sample.power.efficiency_freq_mhz = g_io_report_freq.efficiency_freq_mhz;
    if (!sample.power.performance_freq_mhz && g_io_report_freq.performance_freq_mhz && *g_io_report_freq.performance_freq_mhz > 0) sample.power.performance_freq_mhz = g_io_report_freq.performance_freq_mhz;

    return sample;
}

int main() {
    openSmcConnection();
    g_io_report_power.init();
    g_io_report_freq.init();
    g_powermetrics_sampler.startCapture();

    std::deque<double> cpu_history;
    std::deque<double> gpu_history;
    std::deque<double> power_history;
    std::deque<double> memory_history;

    initializeCursesUi();
    bool is_running = true;
    while (is_running) {
        auto tick_start = std::chrono::steady_clock::now();
        SystemSample sample = collectSystemSample();
        pushToHistory(cpu_history, sample.cpu_average_pct);
        pushToHistory(gpu_history, sample.power.gpu_usage_pct.value_or(0.0));
        pushRawToHistory(power_history, sample.power.system_power_watts.value_or(0.0));
        pushToHistory(memory_history, sample.memory.used_pct);
        renderUiView(sample, cpu_history, gpu_history, power_history, memory_history);

        auto sleep_until = tick_start + std::chrono::milliseconds(REFRESH_INTERVAL_MS);
        while (std::chrono::steady_clock::now() < sleep_until) {
            int ch = getch();
            if (ch == 'q' || ch == 'Q') {
                is_running = false;
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }
    }
    closeSmcConnection();
    endwin();
    return 0;
}
