#include "format.hpp"

#include <array>

std::string formatBytes(uint64_t bytes) {
    const std::array<const char*, 5> units = {"B", "KB", "MB", "GB", "TB"};
    double value = static_cast<double>(bytes);
    size_t unit = 0;
    while (value >= 1024.0 && unit + 1 < units.size()) {
        value /= 1024.0;
        ++unit;
    }
    std::ostringstream out;
    out.setf(std::ios::fixed);
    out.precision(unit == 0 ? 0 : 1);
    out << value << units[unit];
    return out.str();
}

std::string cfString(CFTypeRef value) {
    if (!value || CFGetTypeID(value) != CFStringGetTypeID()) return {};
    char buffer[256]{};
    if (!CFStringGetCString(static_cast<CFStringRef>(value), buffer, sizeof(buffer), kCFStringEncodingUTF8)) return {};
    return buffer;
}
