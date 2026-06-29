#include "io_report.hpp"

#include <dlfcn.h>

PfnCopyAllChannels g_pfn_copy_all_channels = nullptr;
PfnCreateSubscription g_pfn_create_subscription = nullptr;
PfnCreateSamples g_pfn_create_samples = nullptr;
PfnCreateDelta g_pfn_create_delta = nullptr;
PfnGetGroup g_pfn_get_group = nullptr;
PfnGetSubGroup g_pfn_get_sub_group = nullptr;
PfnGetChannelName g_pfn_get_channel_name = nullptr;
PfnGetIntValue g_pfn_get_int_value = nullptr;
PfnGetUnitLabel g_pfn_get_unit_label = nullptr;
PfnStateCount g_pfn_state_count = nullptr;
PfnStateName g_pfn_state_name = nullptr;
PfnStateResidency g_pfn_state_residency = nullptr;

std::string cfStringToStdString(CFStringRef s) {
    if (!s) return {};
    char buf[128] = {};
    CFStringGetCString(s, buf, sizeof(buf), kCFStringEncodingUTF8);
    return buf;
}

bool loadIOReport() {
    static bool loaded = false;
    if (loaded) return g_pfn_create_samples != nullptr;
    void* h = dlopen("/System/Library/PrivateFrameworks/IOReport.framework/IOReport", RTLD_LAZY);
    if (!h) h = RTLD_DEFAULT;
    g_pfn_copy_all_channels = reinterpret_cast<PfnCopyAllChannels>(dlsym(h, "IOReportCopyAllChannels"));
    g_pfn_create_subscription = reinterpret_cast<PfnCreateSubscription>(dlsym(h, "IOReportCreateSubscription"));
    g_pfn_create_samples = reinterpret_cast<PfnCreateSamples>(dlsym(h, "IOReportCreateSamples"));
    g_pfn_create_delta = reinterpret_cast<PfnCreateDelta>(dlsym(h, "IOReportCreateSamplesDelta"));
    g_pfn_get_group = reinterpret_cast<PfnGetGroup>(dlsym(h, "IOReportChannelGetGroup"));
    g_pfn_get_sub_group = reinterpret_cast<PfnGetSubGroup>(dlsym(h, "IOReportChannelGetSubGroup"));
    g_pfn_get_channel_name = reinterpret_cast<PfnGetChannelName>(dlsym(h, "IOReportChannelGetChannelName"));
    g_pfn_get_int_value = reinterpret_cast<PfnGetIntValue>(dlsym(h, "IOReportSimpleGetIntegerValue"));
    g_pfn_get_unit_label = reinterpret_cast<PfnGetUnitLabel>(dlsym(h, "IOReportChannelGetUnitLabel"));
    g_pfn_state_count = reinterpret_cast<PfnStateCount>(dlsym(h, "IOReportStateGetCount"));
    g_pfn_state_name = reinterpret_cast<PfnStateName>(dlsym(h, "IOReportStateGetNameForIndex"));
    g_pfn_state_residency = reinterpret_cast<PfnStateResidency>(dlsym(h, "IOReportStateGetResidency"));
    loaded = true;
    return g_pfn_create_samples != nullptr;
}

bool IOReport::init(std::function<bool(const std::string&, const std::string&, const std::string&, const std::string&)> filter) {
    if (!loadIOReport() || !g_pfn_copy_all_channels) return false;
    auto all = g_pfn_copy_all_channels(0, 0);
    if (!all) return false;

    CFMutableDictionaryRef mutableDict = CFDictionaryCreateMutableCopy(kCFAllocatorDefault, CFDictionaryGetCount(all), all);
    CFArrayRef channels = static_cast<CFArrayRef>(CFDictionaryGetValue(all, CFSTR("IOReportChannels")));
    if (!channels) { CFRelease(all); CFRelease(mutableDict); return false; }

    CFMutableArrayRef selected = CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks);
    CFIndex count = CFArrayGetCount(channels);
    for (CFIndex i = 0; i < count; ++i) {
        auto item = static_cast<CFDictionaryRef>(CFArrayGetValueAtIndex(channels, i));
        std::string group = cfStringToStdString(g_pfn_get_group(item));
        std::string subG = cfStringToStdString(g_pfn_get_sub_group ? g_pfn_get_sub_group(item) : nullptr);
        std::string chName = cfStringToStdString(g_pfn_get_channel_name(item));
        std::string unit = cfStringToStdString(g_pfn_get_unit_label(item));
        if (filter(group, subG, chName, unit)) {
            CFArrayAppendValue(selected, item);
        }
    }

    CFDictionarySetValue(mutableDict, CFSTR("IOReportChannels"), selected);
    CFRelease(selected);

    CFMutableDictionaryRef rawOut = nullptr;
    subscription = g_pfn_create_subscription(nullptr, mutableDict, &rawOut, 0, nullptr);
    if (rawOut) CFRelease(rawOut);
    CFRelease(all);
    if (!subscription) { CFRelease(mutableDict); return false; }

    channels_dict = mutableDict;
    previous_sample = g_pfn_create_samples(subscription, nullptr, nullptr);
    previous_time = std::chrono::steady_clock::now();
    return previous_sample != nullptr;
}

std::pair<CFDictionaryRef, uint64_t> IOReport::takeDelta() {
    auto now = std::chrono::steady_clock::now();
    auto sample = g_pfn_create_samples(subscription, nullptr, nullptr);
    uint64_t elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - previous_time).count();
    auto delta = g_pfn_create_delta(previous_sample, sample, nullptr);
    CFRelease(previous_sample);
    previous_sample = sample;
    previous_time = now;
    return {delta, elapsed};
}
