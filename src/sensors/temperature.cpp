#include "temperature.hpp"

#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/hid/IOHIDDeviceKeys.h>
#include <IOKit/hidsystem/IOHIDEventSystemClient.h>
#include <IOKit/hidsystem/IOHIDServiceClient.h>
#include <dlfcn.h>

#include <algorithm>
#include <cstdio>
#include <string>

#include "../smc/smc.hpp"
#include "../utils/format.hpp"

void applyTemperatureGroup(PowerMetrics& metrics, const std::vector<double>& values, bool is_gpu) {
    if (values.empty()) return;
    double sum = 0.0;
    for (double value : values) sum += value;
    double avg = sum / static_cast<double>(values.size());
    double hot = *std::max_element(values.begin(), values.end());
    if (is_gpu) {
        metrics.gpu_avg_temp_celsius = metrics.gpu_avg_temp_celsius.value_or(avg);
        metrics.gpu_hot_temp_celsius = metrics.gpu_hot_temp_celsius.value_or(hot);
    } else {
        metrics.cpu_avg_temp_celsius = metrics.cpu_avg_temp_celsius.value_or(avg);
        metrics.cpu_hot_temp_celsius = metrics.cpu_hot_temp_celsius.value_or(hot);
    }
}

TemperatureGroups readHidTemperatures() {
    TemperatureGroups result;
    using PfnCreateClient = IOHIDEventSystemClientRef (*)(CFAllocatorRef);
    using PfnSetMatching = void (*)(IOHIDEventSystemClientRef, CFDictionaryRef);
    using PfnCopyEvent = CFTypeRef (*)(IOHIDServiceClientRef, int64_t, CFDictionaryRef, uint32_t);
    using PfnFloatValue = double (*)(CFTypeRef, int32_t);
    auto create_client = reinterpret_cast<PfnCreateClient>(dlsym(RTLD_DEFAULT, "IOHIDEventSystemClientCreate"));
    auto set_matching = reinterpret_cast<PfnSetMatching>(dlsym(RTLD_DEFAULT, "IOHIDEventSystemClientSetMatching"));
    auto copy_event = reinterpret_cast<PfnCopyEvent>(dlsym(RTLD_DEFAULT, "IOHIDServiceClientCopyEvent"));
    auto float_value = reinterpret_cast<PfnFloatValue>(dlsym(RTLD_DEFAULT, "IOHIDEventGetFloatValue"));
    if (!copy_event || !float_value) return result;

    IOHIDEventSystemClientRef client = create_client ? create_client(kCFAllocatorDefault)
                                                     : IOHIDEventSystemClientCreateSimpleClient(kCFAllocatorDefault);
    if (!client) return result;

    if (set_matching) {
        CFMutableDictionaryRef match = CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
            &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
        int usage_page = 0xff00;
        int usage = 5;
        CFNumberRef page_ref = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &usage_page);
        CFNumberRef usage_ref = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &usage);
        CFDictionarySetValue(match, CFSTR("PrimaryUsagePage"), page_ref);
        CFDictionarySetValue(match, CFSTR("PrimaryUsage"), usage_ref);
        set_matching(client, match);
        CFRelease(page_ref);
        CFRelease(usage_ref);
        CFRelease(match);
    }

    CFArrayRef services = IOHIDEventSystemClientCopyServices(client);
    if (!services) {
        CFRelease(client);
        return result;
    }

    constexpr int64_t TEMPERATURE_EVENT_TYPE = 15;
    constexpr int32_t TEMPERATURE_FIELD = static_cast<int32_t>(TEMPERATURE_EVENT_TYPE << 16);
    for (CFIndex i = 0; i < CFArrayGetCount(services); ++i) {
        auto service = const_cast<IOHIDServiceClientRef>(
            static_cast<const __IOHIDServiceClient*>(CFArrayGetValueAtIndex(services, i)));
        CFTypeRef product_ref = IOHIDServiceClientCopyProperty(service, CFSTR(kIOHIDProductKey));
        std::string product_raw = cfString(product_ref);
        std::string product = toLowerCopy(product_raw);
        if (product_ref) CFRelease(product_ref);
        if (product.empty()) continue;

        CFTypeRef event = copy_event(service, TEMPERATURE_EVENT_TYPE, nullptr, 0);
        if (!event) continue;
        double value = float_value(event, TEMPERATURE_FIELD);
        CFRelease(event);
        if (value <= 0.0 || value >= 150.0) continue;

        bool is_gpu = product.find("gpu") != std::string::npos ||
                     (product.find("pmu tp") != std::string::npos && !product.empty() && product.back() == 'g');
        bool is_cpu = !is_gpu && containsAny(product, {"eacc", "pacc", "pmu tdie", "soc mtr temp", "cpu"});

        if (!is_gpu && !is_cpu) {
            FILE* dbg = fopen("/tmp/pulse_debug.log", "a");
            if (dbg) { fprintf(dbg, "  UNCLASSIFIED: '%s' = %.1f\n", product_raw.c_str(), value); fclose(dbg); }
        }

        if (is_gpu) result.gpu_temps.push_back(value);
        if (is_cpu) result.cpu_temps.push_back(value);
    }

    CFRelease(services);
    CFRelease(client);
    return result;
}

TemperatureGroups readSmcTemperatures() {
    TemperatureGroups result;
    if (!openSmcConnection()) return result;

    static const char* cpu_keys[] = {
        "Tp01","Tp02","Tp03","Tp04","Tp05","Tp06","Tp07","Tp08",
        "Tp09","Tp0A","Tp0B","Tp0C","Tp0D","Tp0E","Tp0F","Tp10",
        "Tp11","Tp12","Tp13","Tp14","Tp15","Tp16","Tp17","Tp18",
        "Te01","Te02","Te03","Te04","Te05","Te06","Te07","Te08",
        "Te09","Te0A","Te0B","Te0C","Te0D","Te0E","Te0F","Te10",
        "TC0P","TC0D","TC0E","TC0F","TC1C","TC2C","TC3C","TC4C",
        nullptr
    };
    static const char* gpu_keys[] = {
        "Tg01","Tg02","Tg03","Tg04","Tg05","Tg06","Tg07","Tg08",
        "Tg09","Tg0A","Tg0B","Tg0C","Tg0D","Tg0E","Tg0F","Tg10",
        "TG0P","TG0D","TG1D",
        nullptr
    };

    FILE* dbg = nullptr;
    static bool first_run = true;
    if (first_run) { first_run = false; dbg = fopen("/tmp/pulse_debug.log", "a"); }

    for (int i = 0; cpu_keys[i]; ++i) {
        auto temp = readSmcKey(packSmcKey(cpu_keys[i]));
        if (temp && *temp > -40.0 && *temp < 130.0) {
            result.cpu_temps.push_back(*temp);
            if (dbg) fprintf(dbg, "  SMC cpu '%s' = %.1f\n", cpu_keys[i], *temp);
        }
    }
    for (int i = 0; gpu_keys[i]; ++i) {
        auto temp = readSmcKey(packSmcKey(gpu_keys[i]));
        if (temp && *temp > -40.0 && *temp < 130.0) {
            result.gpu_temps.push_back(*temp);
            if (dbg) fprintf(dbg, "  SMC gpu '%s' = %.1f\n", gpu_keys[i], *temp);
        }
    }
    if (dbg) {
        fprintf(dbg, "SMC temps: cpu=%zu gpu=%zu\n", result.cpu_temps.size(), result.gpu_temps.size());
        fclose(dbg);
    }
    return result;
}
