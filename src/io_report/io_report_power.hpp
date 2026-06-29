#pragma once

#include <optional>

#include "io_report.hpp"

struct IOReportPower {
    IOReport report;
    std::optional<double> cpu_power_watts;
    std::optional<double> gpu_power_watts;

    bool init();
    void collect();
};

extern IOReportPower g_io_report_power;
