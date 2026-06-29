#include "io_report_power.hpp"

#include <string>
#include <vector>

IOReportPower g_io_report_power;

bool IOReportPower::init() {
    return report.init([](const std::string& group, const std::string&, const std::string&, const std::string&) {
        return group == "Energy Model";
    });
}

void IOReportPower::collect() {
    if (!report.subscription || !report.previous_sample) return;
    auto [delta, elapsed] = report.takeDelta();
    if (!delta || elapsed == 0) { if (delta) CFRelease(delta); return; }

    double cpu_energy = 0, gpu_energy = 0;
    CFIndex count = CFDictionaryGetCount(delta);
    if (count <= 0) { CFRelease(delta); return; }
    std::vector<const void*> keys(count), vals(count);
    CFDictionaryGetKeysAndValues(delta, keys.data(), vals.data());
    for (CFIndex i = 0; i < count; ++i) {
        auto item = static_cast<CFDictionaryRef>(vals[i]);
        std::string group = cfStringToStdString(g_pfn_get_group(item));
        if (group != "Energy Model") continue;
        std::string name = cfStringToStdString(g_pfn_get_channel_name(item));
        std::string unit = cfStringToStdString(g_pfn_get_unit_label(item));
        int64_t raw = g_pfn_get_int_value(item, 0);
        double watts = static_cast<double>(raw) / (static_cast<double>(elapsed));
        if (unit == "mJ") watts /= 1e3;
        else if (unit == "uJ") watts /= 1e6;
        else if (unit == "nJ") watts /= 1e9;
        if (name.find("CPU") != std::string::npos) cpu_energy += watts;
        else if (name.find("GPU") != std::string::npos) gpu_energy += watts;
    }
    CFRelease(delta);
    if (cpu_energy > 0) cpu_power_watts = cpu_energy;
    if (gpu_energy > 0) gpu_power_watts = gpu_energy;
}
