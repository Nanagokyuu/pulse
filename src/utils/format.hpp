#pragma once

#include <CoreFoundation/CoreFoundation.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <initializer_list>
#include <optional>
#include <sstream>
#include <string>

inline double clampPercentage(double v) {
    if (std::isnan(v) || std::isinf(v)) return 0.0;
    return std::clamp(v, 0.0, 100.0);
}

inline std::string formatNumber(std::optional<double> value, const char* suffix, int precision = 1) {
    if (!value || std::isnan(*value) || std::isinf(*value)) return "N/A";
    std::ostringstream out;
    out.setf(std::ios::fixed);
    out.precision(precision);
    out << *value << suffix;
    return out.str();
}

inline std::string toLowerCopy(std::string text) {
    std::transform(text.begin(), text.end(), text.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return text;
}

inline bool containsAny(const std::string& text, std::initializer_list<const char*> needles) {
    for (const char* needle : needles) {
        if (text.find(needle) != std::string::npos) return true;
    }
    return false;
}

std::string formatBytes(uint64_t bytes);
std::string cfString(CFTypeRef value);
