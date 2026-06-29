#include "battery.hpp"

#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/ps/IOPowerSources.h>
#include <IOKit/ps/IOPSKeys.h>

#include <cmath>
#include <optional>

#include "../smc/smc.hpp"
#include "../utils/format.hpp"

static std::optional<double> cfNumberToDouble(CFDictionaryRef dict, CFStringRef key) {
    auto value = static_cast<CFNumberRef>(CFDictionaryGetValue(dict, key));
    if (!value || CFGetTypeID(value) != CFNumberGetTypeID()) return std::nullopt;
    double result = 0.0;
    if (!CFNumberGetValue(value, kCFNumberDoubleType, &result)) return std::nullopt;
    return result;
}

static std::optional<bool> cfBoolValue(CFDictionaryRef dict, CFStringRef key) {
    auto value = static_cast<CFBooleanRef>(CFDictionaryGetValue(dict, key));
    if (!value || CFGetTypeID(value) != CFBooleanGetTypeID()) return std::nullopt;
    return CFBooleanGetValue(value);
}

BatteryMetrics readBatteryMetrics() {
    BatteryMetrics result;

    CFTypeRef info = IOPSCopyPowerSourcesInfo();
    if (info) {
        CFArrayRef sources = IOPSCopyPowerSourcesList(info);
        if (sources) {
            CFIndex count = CFArrayGetCount(sources);
            for (CFIndex i = 0; i < count; ++i) {
                auto source = static_cast<CFTypeRef>(CFArrayGetValueAtIndex(sources, i));
                auto description = IOPSGetPowerSourceDescription(info, source);
                if (!description) continue;
                auto dict = static_cast<CFDictionaryRef>(description);
                auto current = cfNumberToDouble(dict, CFSTR(kIOPSCurrentCapacityKey));
                auto max = cfNumberToDouble(dict, CFSTR(kIOPSMaxCapacityKey));
                if (current && max && *max > 0.0) result.capacity_pct = clampPercentage((*current / *max) * 100.0);
                auto charging = cfBoolValue(dict, CFSTR(kIOPSIsChargingKey));
                if (charging) result.is_charging = *charging;
            }
            CFRelease(sources);
        }
        CFRelease(info);
    }

    io_service_t battery = IOServiceGetMatchingService(kIOMainPortDefault, IOServiceMatching("AppleSmartBattery"));
    if (battery) {
        CFMutableDictionaryRef props = nullptr;
        if (IORegistryEntryCreateCFProperties(battery, &props, kCFAllocatorDefault, 0) == KERN_SUCCESS && props) {
            auto voltage_mv = cfNumberToDouble(props, CFSTR("Voltage"));
            auto amperage_ma = cfNumberToDouble(props, CFSTR("Amperage"));
            std::optional<double> raw_battery_power_w;
            if (voltage_mv && amperage_ma) {
                raw_battery_power_w = (*voltage_mv * *amperage_ma) / 1000000.0;
            }

            if (openSmcConnection()) {
                if (auto battery_power = readSmcPower("PPBR")) raw_battery_power_w = *battery_power;
                if (auto input_power = readSmcPower("PDTR"); input_power && *input_power >= 0.0) {
                    result.input_power_watts = *input_power;
                }
            }

            if (raw_battery_power_w) {
                if (result.is_charging) result.battery_power_watts = 0.0;
                else result.battery_power_watts = std::abs(*raw_battery_power_w);
            }
            CFRelease(props);
        }
        IOObjectRelease(battery);
    }

    return result;
}
