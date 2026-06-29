#include "io_report_freq.hpp"

#include <IOKit/IOKitLib.h>

#include <string>

IOReportFreq g_io_report_freq;

bool IOReportFreq::init() {
    readDvfsTable();
    if (efficiency_freqs.empty() && performance_freqs.empty()) return false;
    return report.init([](const std::string& group, const std::string& subgroup, const std::string&, const std::string&) {
        return group == "CPU Stats" && subgroup == "CPU Complex Performance States";
    });
}

void IOReportFreq::collect() {
    if (!report.subscription || !report.previous_sample) return;
    auto [delta, elapsed] = report.takeDelta();
    if (!delta) return;

    CFIndex count = CFDictionaryGetCount(delta);
    if (count <= 0) { CFRelease(delta); return; }
    std::vector<const void*> keys(count), vals(count);
    CFDictionaryGetKeysAndValues(delta, keys.data(), vals.data());
    for (CFIndex i = 0; i < count; ++i) {
        auto item = static_cast<CFDictionaryRef>(vals[i]);
        std::string name = cfStringToStdString(g_pfn_get_channel_name(item));
        int state_count = g_pfn_state_count(item);
        if (state_count < 2) continue;
        int offset = -1;
        for (int s = 0; s < state_count; ++s) {
            auto state_name = cfStringToStdString(g_pfn_state_name(item, s));
            if (state_name != "IDLE" && state_name != "DOWN" && state_name != "OFF") { offset = s; break; }
        }
        if (offset < 0) continue;
        const auto& freqs = name.find("ECPU") != std::string::npos ? efficiency_freqs :
                            name.find("PCPU") != std::string::npos ? performance_freqs : std::vector<int>();
        if (freqs.empty()) continue;
        double active_total = 0;
        for (int s = offset; s < state_count; ++s) active_total += static_cast<double>(g_pfn_state_residency(item, s));
        if (active_total <= 0) continue;
        double avg = 0;
        for (size_t fi = 0; fi < freqs.size(); ++fi) {
            int si = static_cast<int>(fi) + offset;
            if (si >= state_count) break;
            avg += (static_cast<double>(g_pfn_state_residency(item, si)) / active_total) * static_cast<double>(freqs[fi]);
        }
        if (name.find("ECPU") != std::string::npos) efficiency_freq_mhz = avg;
        else if (name.find("PCPU") != std::string::npos) performance_freq_mhz = avg;
    }
    CFRelease(delta);
}

void IOReportFreq::readDvfsTable() {
    io_service_t svc = IOServiceGetMatchingService(kIOMainPortDefault, IOServiceMatching("AppleARMIODevice"));
    if (!svc) return;
    io_iterator_t iter;
    if (IORegistryEntryGetChildIterator(svc, kIOServicePlane, &iter) != KERN_SUCCESS) { IOObjectRelease(svc); return; }
    io_object_t child;
    while ((child = IOIteratorNext(iter)) != 0) {
        CFMutableDictionaryRef props = nullptr;
        if (IORegistryEntryCreateCFProperties(child, &props, kCFAllocatorDefault, 0) == KERN_SUCCESS && props) {
            auto e_data = static_cast<CFDataRef>(CFDictionaryGetValue(props, CFSTR("voltage-states1-sram")));
            auto p_data = static_cast<CFDataRef>(CFDictionaryGetValue(props, CFSTR("voltage-states5-sram")));
            if (e_data) efficiency_freqs = parseDvfs(e_data);
            if (p_data) performance_freqs = parseDvfs(p_data);
            CFRelease(props);
            if (!efficiency_freqs.empty() || !performance_freqs.empty()) { IOObjectRelease(child); break; }
        }
        IOObjectRelease(child);
    }
    IOObjectRelease(iter);
    IOObjectRelease(svc);
}

std::vector<int> IOReportFreq::parseDvfs(CFDataRef data) {
    std::vector<int> freqs;
    if (!data) return freqs;
    const uint8_t* bytes = CFDataGetBytePtr(data);
    CFIndex len = CFDataGetLength(data);
    for (CFIndex i = 0; i + 7 < len; i += 8) {
        uint32_t hz = bytes[i] | (bytes[i+1] << 8) | (bytes[i+2] << 16) | (bytes[i+3] << 24);
        if (hz > 0) freqs.push_back(static_cast<int>(hz / 1000000));
    }
    return freqs;
}
