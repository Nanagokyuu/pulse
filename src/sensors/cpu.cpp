#include "cpu.hpp"

#include <mach/host_info.h>
#include <mach/mach_host.h>
#include <mach/processor_info.h>
#include <mach/vm_map.h>

#include <cstdint>

#include "../core/types.hpp"
#include "../utils/format.hpp"

static std::vector<CpuTicks> g_previous_cpu_ticks;

std::vector<double> readCpuUsage() {
    processor_info_array_t cpu_info = nullptr;
    mach_msg_type_number_t cpu_info_count = 0;
    natural_t cpu_count = 0;

    kern_return_t kr = host_processor_info(mach_host_self(), PROCESSOR_CPU_LOAD_INFO, &cpu_count, &cpu_info, &cpu_info_count);
    if (kr != KERN_SUCCESS || cpu_info == nullptr) return {};

    std::vector<CpuTicks> current(cpu_count);
    for (natural_t i = 0; i < cpu_count; ++i) {
        auto* load = reinterpret_cast<processor_cpu_load_info_t>(cpu_info)[i].cpu_ticks;
        current[i] = {
            static_cast<uint64_t>(load[CPU_STATE_USER]),
            static_cast<uint64_t>(load[CPU_STATE_SYSTEM]),
            static_cast<uint64_t>(load[CPU_STATE_IDLE]),
            static_cast<uint64_t>(load[CPU_STATE_NICE]),
        };
    }

    std::vector<double> usage(cpu_count, 0.0);
    if (g_previous_cpu_ticks.size() == current.size()) {
        for (size_t i = 0; i < current.size(); ++i) {
            const auto& now = current[i];
            const auto& old = g_previous_cpu_ticks[i];
            uint64_t active = (now.user_ticks - old.user_ticks) + (now.system_ticks - old.system_ticks) + (now.nice_ticks - old.nice_ticks);
            uint64_t idle = now.idle_ticks - old.idle_ticks;
            uint64_t total = active + idle;
            usage[i] = total == 0 ? 0.0 : clampPercentage(100.0 * static_cast<double>(active) / static_cast<double>(total));
        }
    }

    g_previous_cpu_ticks = std::move(current);
    vm_deallocate(mach_task_self(), reinterpret_cast<vm_address_t>(cpu_info), cpu_info_count * sizeof(integer_t));
    return usage;
}
