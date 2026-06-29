#pragma once

#include <string>
#include <vector>

#include "../core/types.hpp"

void applyTemperatureGroup(PowerMetrics& metrics, const std::vector<double>& values, bool is_gpu);
TemperatureGroups readHidTemperatures();
TemperatureGroups readSmcTemperatures();
