#pragma once

#include <optional>
#include <vector>

#include "io_report.hpp"

struct IOReportFreq {
    IOReport report;
    std::vector<int> efficiency_freqs, performance_freqs;
    std::optional<double> efficiency_freq_mhz, performance_freq_mhz;

    bool init();
    void collect();

private:
    void readDvfsTable();
    static std::vector<int> parseDvfs(CFDataRef data);
};

extern IOReportFreq g_io_report_freq;
