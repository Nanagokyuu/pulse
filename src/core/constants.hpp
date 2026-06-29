#pragma once

#include <cstddef>
#include <cstdint>

constexpr int REFRESH_INTERVAL_MS = 1000;
constexpr int POWERMETRICS_INTERVAL_MS = 1000;
constexpr size_t HISTORY_LIMIT = 240;

constexpr uint32_t SMC_KERNEL_INDEX = 2;
constexpr uint32_t SMC_CMD_READ_BYTES = 5;
constexpr uint32_t SMC_CMD_READ_KEY_INFO = 9;
