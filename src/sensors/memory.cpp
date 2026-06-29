#include "memory.hpp"

#include <mach/host_info.h>
#include <mach/mach_host.h>
#include <mach/vm_statistics.h>
#include <sys/sysctl.h>
#include <unistd.h>

#include "../utils/format.hpp"

MemoryMetrics readMemoryMetrics() {
    MemoryMetrics result;
    uint64_t total = 0;
    size_t total_size = sizeof(total);
    sysctlbyname("hw.memsize", &total, &total_size, nullptr, 0);

    vm_statistics64_data_t vm_stats{};
    mach_msg_type_number_t count = HOST_VM_INFO64_COUNT;
    if (host_statistics64(mach_host_self(), HOST_VM_INFO64, reinterpret_cast<host_info64_t>(&vm_stats), &count) == KERN_SUCCESS) {
        uint64_t page_size = static_cast<uint64_t>(getpagesize());
        uint64_t free_pages = vm_stats.free_count;
        uint64_t speculative_pages = vm_stats.speculative_count;
        uint64_t cached_pages = vm_stats.external_page_count + vm_stats.purgeable_count;
        uint64_t available_pages = free_pages + speculative_pages + vm_stats.inactive_count;
        uint64_t used_pages = vm_stats.active_count + vm_stats.wire_count + vm_stats.compressor_page_count;

        result.free_bytes = free_pages * page_size;
        result.cached_bytes = cached_pages * page_size;
        result.available_bytes = available_pages * page_size;
        result.used_bytes = used_pages * page_size;
    }
    result.total_bytes = total;
    result.used_pct = total == 0 ? 0.0 : clampPercentage(100.0 * static_cast<double>(result.used_bytes) / static_cast<double>(total));

    xsw_usage swap{};
    size_t swap_size = sizeof(swap);
    if (sysctlbyname("vm.swapusage", &swap, &swap_size, nullptr, 0) == 0) {
        result.swap_used_bytes = swap.xsu_used;
        result.swap_total_bytes = swap.xsu_total;
    }
    return result;
}
