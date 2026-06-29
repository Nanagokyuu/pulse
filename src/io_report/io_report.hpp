#pragma once

#include <CoreFoundation/CoreFoundation.h>

#include <chrono>
#include <cstdint>
#include <functional>
#include <string>
#include <utility>

using IOReportSubscriptionRef = const void*;
using PfnCopyAllChannels = CFDictionaryRef (*)(uint64_t, uint64_t);
using PfnCreateSubscription = IOReportSubscriptionRef (*)(const void*, CFMutableDictionaryRef, CFMutableDictionaryRef*, uint64_t, CFTypeRef);
using PfnCreateSamples = CFDictionaryRef (*)(IOReportSubscriptionRef, CFMutableDictionaryRef, CFTypeRef);
using PfnCreateDelta = CFDictionaryRef (*)(CFDictionaryRef, CFDictionaryRef, CFTypeRef);
using PfnGetGroup = CFStringRef (*)(CFDictionaryRef);
using PfnGetSubGroup = CFStringRef (*)(CFDictionaryRef);
using PfnGetChannelName = CFStringRef (*)(CFDictionaryRef);
using PfnGetIntValue = int64_t (*)(CFDictionaryRef, int32_t);
using PfnGetUnitLabel = CFStringRef (*)(CFDictionaryRef);
using PfnStateCount = int32_t (*)(CFDictionaryRef);
using PfnStateName = CFStringRef (*)(CFDictionaryRef, int32_t);
using PfnStateResidency = int64_t (*)(CFDictionaryRef, int32_t);

extern PfnCopyAllChannels g_pfn_copy_all_channels;
extern PfnCreateSubscription g_pfn_create_subscription;
extern PfnCreateSamples g_pfn_create_samples;
extern PfnCreateDelta g_pfn_create_delta;
extern PfnGetGroup g_pfn_get_group;
extern PfnGetSubGroup g_pfn_get_sub_group;
extern PfnGetChannelName g_pfn_get_channel_name;
extern PfnGetIntValue g_pfn_get_int_value;
extern PfnGetUnitLabel g_pfn_get_unit_label;
extern PfnStateCount g_pfn_state_count;
extern PfnStateName g_pfn_state_name;
extern PfnStateResidency g_pfn_state_residency;

std::string cfStringToStdString(CFStringRef s);
bool loadIOReport();

struct IOReport {
    IOReportSubscriptionRef subscription = nullptr;
    CFMutableDictionaryRef channels_dict = nullptr;
    CFDictionaryRef previous_sample = nullptr;
    std::chrono::steady_clock::time_point previous_time;

    bool init(std::function<bool(const std::string&, const std::string&, const std::string&, const std::string&)> filter);
    std::pair<CFDictionaryRef, uint64_t> takeDelta();
};
