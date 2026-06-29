#pragma once

#include <deque>
#include <mutex>
#include <string>
#include <thread>

#include "../core/types.hpp"

std::optional<double> extractFirstNumber(const std::string& line);
std::optional<double> extractNumberAfterColon(const std::string& line);
std::optional<double> parseValueAsWatts(const std::string& line);
std::string runPowermetrics(int interval_ms = 1000);
PowerMetrics parsePowermetricsOutput(const std::string& text);

struct PowermetricsSampler {
    std::mutex mutex;
    PowerMetrics latest_metrics;
    std::jthread worker;

    void startCapture();
    PowerMetrics takeSnapshot();
};

extern PowermetricsSampler g_powermetrics_sampler;

void pushToHistory(std::deque<double>& history, double value);
void pushRawToHistory(std::deque<double>& history, double value);
