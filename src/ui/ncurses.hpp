#pragma once

#include <deque>
#include <string>
#include <vector>

#include "../core/types.hpp"

void initializeCursesUi();
void drawText(int y, int x, const std::string& text, int attr = 0);
void drawBox(int y, int x, int h, int w, const std::string& title, int colorPair);
void drawHorizontalBar(int y, int x, int width, double pct, int colorPair);
void drawLineChart(int y, int x, int h, int w, const std::deque<double>& history, int colorPair);
void drawLineChartScaled(int y, int x, int h, int w, const std::deque<double>& history, int colorPair, double maxVal);
void drawMetricLine(int y, int x, const std::string& label, const std::string& value);
void drawCoreGrid(int y, int x, int h, int w, const std::vector<double>& cores);
void renderUiView(const SystemSample& sample, const std::deque<double>& cpu_history, const std::deque<double>& gpu_history,
                  const std::deque<double>& power_history, const std::deque<double>& memory_history);
