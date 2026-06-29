#include "powermetrics.hpp"

#include <unistd.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <deque>
#include <map>
#include <optional>
#include <regex>
#include <sstream>
#include <vector>

#include "../core/constants.hpp"
#include "../utils/format.hpp"

PowermetricsSampler g_powermetrics_sampler;

std::optional<double> extractFirstNumber(const std::string& line) {
    static const std::regex number(R"((-?\d+(?:\.\d+)?))");
    std::smatch match;
    if (!std::regex_search(line, match, number)) return std::nullopt;
    return std::stod(match[1].str());
}

std::optional<double> extractNumberAfterColon(const std::string& line) {
    size_t colon = line.find(':');
    if (colon == std::string::npos || colon + 1 >= line.size()) return extractFirstNumber(line);
    return extractFirstNumber(line.substr(colon + 1));
}

std::optional<double> parseValueAsWatts(const std::string& line) {
    auto value = extractNumberAfterColon(line);
    if (!value) return std::nullopt;
    std::string lower = line;
    std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    if (lower.find("mw") != std::string::npos) return *value / 1000.0;
    if (lower.find("uw") != std::string::npos) return *value / 1000000.0;
    return *value;
}

std::string runPowermetrics(int interval_ms) {
    if (geteuid() != 0) return "powermetrics must be invoked as the superuser";
    std::array<char, 512> buffer{};
    std::string output;
    std::ostringstream cmd;
    cmd << "powermetrics -n 1 -i " << interval_ms
        << " --samplers cpu_power,gpu_power,thermal,battery --show-extra-power-info 2>&1";
    FILE* pipe = popen(cmd.str().c_str(), "r");
    if (!pipe) return {};
    while (fgets(buffer.data(), static_cast<int>(buffer.size()), pipe) != nullptr) {
        output += buffer.data();
    }
    pclose(pipe);

    {
        FILE* f = fopen("/tmp/pulse_powermetrics.txt", "w");
        if (f) { fputs(output.c_str(), f); fclose(f); }
    }
    return output;
}

PowerMetrics parsePowermetricsOutput(const std::string& text) {
    PowerMetrics result;
    if (text.find("must be run as root") != std::string::npos ||
        text.find("must be invoked as the superuser") != std::string::npos ||
        text.find("Operation not permitted") != std::string::npos ||
        text.find("permission") != std::string::npos) {
        result.error = "powermetrics needs sudo for hardware counters";
    }

    std::vector<double> cpu_temps;
    std::vector<double> gpu_temps;
    std::map<std::string, ClusterReading> efficiency_clusters;
    std::map<std::string, ClusterReading> performance_clusters;
    std::istringstream lines(text);
    std::string line;
    while (std::getline(lines, line)) {
        std::string lower = toLowerCopy(line);

        if (containsAny(lower, {"system total power", "total system power"})) {
            if (auto watts = parseValueAsWatts(lower); watts && *watts > 0.0) result.system_power_watts = watts;
        }
        if (containsAny(lower, {"cpu power", "cpu power dissipation"})) {
            if (auto watts = parseValueAsWatts(lower); watts && *watts > 0.0) result.cpu_power_watts = watts;
        }
        if (containsAny(lower, {"gpu power", "gpu power dissipation"})) {
            if (auto watts = parseValueAsWatts(lower); watts && *watts >= 0.0) result.gpu_power_watts = watts;
        }

        if (containsAny(lower, {"cpu die", "cpu temperature", "cpu temp", "package temperature"}) &&
            !containsAny(lower, {"gpu"})) {
            if (auto value = extractNumberAfterColon(lower)) {
                cpu_temps.push_back(*value);
                if (containsAny(lower, {"hot", "max", "peak"})) result.cpu_hot_temp_celsius = *value;
            }
        }
        if (containsAny(lower, {"gpu die", "gpu temperature", "gpu temp"}) &&
            !containsAny(lower, {"cpu"})) {
            if (auto value = extractNumberAfterColon(lower)) {
                gpu_temps.push_back(*value);
                if (containsAny(lower, {"hot", "max", "peak"})) result.gpu_hot_temp_celsius = *value;
            }
        }

        auto update_cluster = [&](std::map<std::string, ClusterReading>& clusters, const std::string& key,
                                 auto ClusterReading::*field, double value) {
            clusters[key].*field = value;
        };

        auto parse_cluster_line = [&](const char* suffix, auto ClusterReading::*field) {
            size_t pos = lower.find(suffix);
            if (pos == std::string::npos) return;
            std::string key = lower.substr(0, pos);
            auto value = extractNumberAfterColon(lower);
            if (!value) return;
            if (key.find("e-cluster") != std::string::npos || key.find("efficiency cluster") != std::string::npos) {
                update_cluster(efficiency_clusters, key, field, *value);
            } else if (key.find("p") != std::string::npos && key.find("cluster") != std::string::npos) {
                update_cluster(performance_clusters, key, field, *value);
            }
        };

        parse_cluster_line(" hw active frequency", &ClusterReading::frequency_mhz);
        parse_cluster_line(" hw active residency", &ClusterReading::active_residency_pct);
        parse_cluster_line(" online", &ClusterReading::online_pct);

        if (containsAny(lower, {"e-cluster", "efficiency cluster", "e cluster"}) &&
            containsAny(lower, {"active frequency", "frequency", "freq"})) {
            if (auto value = extractNumberAfterColon(lower)) result.efficiency_freq_mhz = value;
        }
        if (containsAny(lower, {"p-cluster", "performance cluster", "p cluster"}) &&
            containsAny(lower, {"active frequency", "frequency", "freq"})) {
            if (auto value = extractNumberAfterColon(lower)) result.performance_freq_mhz = value;
        }
        if (lower.find("gpu") != std::string::npos && containsAny(lower, {"frequency", "freq"})) {
            if (auto value = extractNumberAfterColon(lower)) result.gpu_freq_mhz = value;
        }
        if (lower.find("gpu") != std::string::npos && containsAny(lower, {"active residency", "active time", "usage"})) {
            if (auto value = extractNumberAfterColon(lower)) result.gpu_usage_pct = clampPercentage(*value);
        }
    }

    auto aggregate_cluster_frequency = [](const std::map<std::string, ClusterReading>& clusters) -> std::optional<double> {
        double weighted_sum = 0.0;
        double total_weight = 0.0;
        for (const auto& [_, reading] : clusters) {
            if (!reading.frequency_mhz) continue;
            double weight = 0.0;
            if (reading.active_residency_pct && *reading.active_residency_pct > 0.0) weight = *reading.active_residency_pct;
            else if (reading.online_pct && *reading.online_pct > 0.0) weight = *reading.online_pct;
            else weight = 1.0;
            weighted_sum += *reading.frequency_mhz * weight;
            total_weight += weight;
        }
        if (total_weight <= 0.0) return std::nullopt;
        return weighted_sum / total_weight;
    };

    if (auto freq = aggregate_cluster_frequency(efficiency_clusters)) result.efficiency_freq_mhz = *freq;
    if (auto freq = aggregate_cluster_frequency(performance_clusters)) result.performance_freq_mhz = *freq;

    if (!cpu_temps.empty()) {
        double sum = 0.0;
        for (double v : cpu_temps) sum += v;
        result.cpu_avg_temp_celsius = sum / static_cast<double>(cpu_temps.size());
        if (!result.cpu_hot_temp_celsius) result.cpu_hot_temp_celsius = *std::max_element(cpu_temps.begin(), cpu_temps.end());
    }
    if (!gpu_temps.empty()) {
        double sum = 0.0;
        for (double v : gpu_temps) sum += v;
        result.gpu_avg_temp_celsius = sum / static_cast<double>(gpu_temps.size());
        if (!result.gpu_hot_temp_celsius) result.gpu_hot_temp_celsius = *std::max_element(gpu_temps.begin(), gpu_temps.end());
    }

    return result;
}

void PowermetricsSampler::startCapture() {
    worker = std::jthread([this](std::stop_token stop_token) {
        while (!stop_token.stop_requested()) {
            auto started_at = std::chrono::steady_clock::now();
            PowerMetrics sample = parsePowermetricsOutput(runPowermetrics(POWERMETRICS_INTERVAL_MS));
            {
                std::lock_guard<std::mutex> lock(mutex);
                latest_metrics = std::move(sample);
            }

            auto elapsed = std::chrono::steady_clock::now() - started_at;
            auto remaining = std::chrono::milliseconds(POWERMETRICS_INTERVAL_MS) -
                             std::chrono::duration_cast<std::chrono::milliseconds>(elapsed);
            if (remaining > std::chrono::milliseconds(0)) {
                std::this_thread::sleep_for(remaining);
            }
        }
    });
}

PowerMetrics PowermetricsSampler::takeSnapshot() {
    std::lock_guard<std::mutex> lock(mutex);
    return latest_metrics;
}

void pushToHistory(std::deque<double>& history, double value) {
    history.push_back(clampPercentage(value));
    while (history.size() > HISTORY_LIMIT) history.pop_front();
}

void pushRawToHistory(std::deque<double>& history, double value) {
    history.push_back(std::isnan(value) || std::isinf(value) ? 0.0 : value);
    while (history.size() > HISTORY_LIMIT) history.pop_front();
}
