#include "ncurses.hpp"

#include <ncurses.h>

#include <algorithm>
#include <cmath>
#include <sstream>

#include "../utils/format.hpp"

void initializeCursesUi() {
    initscr();
    cbreak();
    noecho();
    curs_set(0);
    keypad(stdscr, TRUE);
    nodelay(stdscr, TRUE);
    use_default_colors();
    start_color();
    init_pair(1, COLOR_CYAN, -1);
    init_pair(2, COLOR_GREEN, -1);
    init_pair(3, COLOR_MAGENTA, -1);
    init_pair(4, COLOR_YELLOW, -1);
    init_pair(5, COLOR_BLUE, -1);
    init_pair(6, COLOR_RED, -1);
}

void drawText(int y, int x, const std::string& text, int attr) {
    if (y < 0 || x < 0 || y >= LINES || x >= COLS) return;
    if (attr) attron(attr);
    mvaddnstr(y, x, text.c_str(), std::max(0, COLS - x - 1));
    if (attr) attroff(attr);
}

void drawBox(int y, int x, int h, int w, const std::string& title, int colorPair) {
    attron(COLOR_PAIR(colorPair));
    mvhline(y, x + 1, ACS_HLINE, std::max(0, w - 2));
    mvhline(y + h - 1, x + 1, ACS_HLINE, std::max(0, w - 2));
    mvvline(y + 1, x, ACS_VLINE, std::max(0, h - 2));
    mvvline(y + 1, x + w - 1, ACS_VLINE, std::max(0, h - 2));
    mvaddch(y, x, ACS_ULCORNER);
    mvaddch(y, x + w - 1, ACS_URCORNER);
    mvaddch(y + h - 1, x, ACS_LLCORNER);
    mvaddch(y + h - 1, x + w - 1, ACS_LRCORNER);
    attroff(COLOR_PAIR(colorPair));
    drawText(y, x + 2, " " + title + " ", A_BOLD | COLOR_PAIR(colorPair));
}

void drawHorizontalBar(int y, int x, int width, double pct, int colorPair) {
    int fill = static_cast<int>(std::round(width * clampPercentage(pct) / 100.0));
    attron(COLOR_PAIR(colorPair));
    for (int i = 0; i < width; ++i) mvaddch(y, x + i, i < fill ? ACS_CKBOARD : ' ');
    attroff(COLOR_PAIR(colorPair));
}

void drawLineChart(int y, int x, int h, int w, const std::deque<double>& history, int colorPair) {
    if (h <= 0 || w <= 0 || history.empty()) return;
    attron(COLOR_PAIR(colorPair));
    for (int row = 0; row < h; ++row) {
        for (int col = 0; col < w; ++col) mvaddch(y + row, x + col, ' ');
    }
    size_t start = history.size() > static_cast<size_t>(w) ? history.size() - static_cast<size_t>(w) : 0;
    for (size_t i = start; i < history.size(); ++i) {
        int col = x + static_cast<int>(i - start);
        int level = static_cast<int>(std::round((h - 1) * history[i] / 100.0));
        int row = y + (h - 1 - std::clamp(level, 0, h - 1));
        mvaddch(row, col, ACS_DIAMOND);
        for (int fillRow = row + 1; fillRow < y + h; ++fillRow) mvaddch(fillRow, col, ACS_CKBOARD);
    }
    attroff(COLOR_PAIR(colorPair));
}

void drawLineChartScaled(int y, int x, int h, int w, const std::deque<double>& history, int colorPair, double maxVal) {
    if (h <= 0 || w <= 0 || history.empty() || maxVal <= 0.0) return;
    attron(COLOR_PAIR(colorPair));
    for (int row = 0; row < h; ++row) {
        for (int col = 0; col < w; ++col) mvaddch(y + row, x + col, ' ');
    }
    size_t start = history.size() > static_cast<size_t>(w) ? history.size() - static_cast<size_t>(w) : 0;
    for (size_t i = start; i < history.size(); ++i) {
        int col = x + static_cast<int>(i - start);
        int level = static_cast<int>(std::round((h - 1) * std::clamp(history[i], 0.0, maxVal) / maxVal));
        int row = y + (h - 1 - std::clamp(level, 0, h - 1));
        mvaddch(row, col, ACS_DIAMOND);
        for (int fillRow = row + 1; fillRow < y + h; ++fillRow) mvaddch(fillRow, col, ACS_CKBOARD);
    }
    attroff(COLOR_PAIR(colorPair));
}

void drawMetricLine(int y, int x, const std::string& label, const std::string& value) {
    drawText(y, x, label + ": ", A_DIM);
    drawText(y, x + static_cast<int>(label.size()) + 2, value, A_BOLD);
}

void drawCoreGrid(int y, int x, int h, int w, const std::vector<double>& cores) {
    if (h <= 0 || w <= 0 || cores.empty()) return;
    int rows = std::max(1, h);
    int cols = static_cast<int>(std::ceil(static_cast<double>(cores.size()) / static_cast<double>(rows)));
    cols = std::clamp(cols, 1, std::max(1, w / 6));
    rows = static_cast<int>(std::ceil(static_cast<double>(cores.size()) / static_cast<double>(cols)));
    rows = std::min(rows, h);
    int colW = std::max(6, w / cols);

    size_t rendered = 0;
    for (size_t i = 0; i < cores.size(); ++i) {
        int col = static_cast<int>(i) / rows;
        int row = static_cast<int>(i) % rows;
        int px = x + col * colW;
        int py = y + row;
        if (py >= y + h || px >= x + w) break;

        std::ostringstream label;
        label << "c" << i;
        if (colW < 9) {
            std::ostringstream compact;
            compact << label.str() << " " << static_cast<int>(std::round(clampPercentage(cores[i])));
            drawText(py, px, compact.str(), A_BOLD);
        } else {
            drawText(py, px, label.str(), A_DIM);
            std::string pct = formatNumber(cores[i], "%", 0);
            int pctX = px + std::max(3, colW - static_cast<int>(pct.size()) - 1);
            int barX = px + 4;
            int barW = std::max(0, pctX - barX - 1);
            if (barW >= 3) drawHorizontalBar(py, barX, barW, cores[i], 2);
            drawText(py, pctX, pct, A_BOLD);
        }
        ++rendered;
    }

    if (rendered < cores.size()) {
        int remaining = static_cast<int>(cores.size() - rendered);
        int lastRow = y + std::min(static_cast<int>(rendered % rows == 0 ? 0 : rendered % rows), h - 1);
        int lastCol = static_cast<int>(rendered) / rows;
        int indicatorX = x + lastCol * colW;
        if (indicatorX < x + w && lastRow < y + h) {
            std::ostringstream overflow;
            overflow << "..." << remaining << " more";
            drawText(lastRow, indicatorX, overflow.str(), A_DIM);
        }
    }
}

void renderUiView(const SystemSample& sample, const std::deque<double>& cpu_history, const std::deque<double>& gpu_history,
                  const std::deque<double>& power_history, const std::deque<double>& memory_history) {
    erase();
    int h = LINES;
    int w = COLS;
    if (h < 18 || w < 64) {
        drawText(0, 0, "pulse needs at least 64x18 terminal space. Press q to quit.", A_BOLD);
        refresh();
        return;
    }

    drawText(0, 2, "pulse", A_BOLD | COLOR_PAIR(1));
    drawText(0, 9, "macOS performance monitor");
    drawText(0, w - 27, "refresh 1000ms | q quit", A_DIM);

    int left_w = w / 2;
    int right_w = w - left_w;
    int top_h = h / 2;
    int bottom_h = h - top_h - 1;

    drawBox(1, 0, top_h, left_w, "CPU", 1);
    drawMetricLine(3, 2, "avg", formatNumber(sample.cpu_average_pct, "%"));
    drawMetricLine(3, 18, "temp", formatNumber(sample.power.cpu_avg_temp_celsius, "C"));
    drawMetricLine(3, 34, "hot", formatNumber(sample.power.cpu_hot_temp_celsius, "C"));
    drawMetricLine(4, 2, "E freq", formatNumber(sample.power.efficiency_freq_mhz, "MHz", 0));
    drawMetricLine(4, 22, "P freq", formatNumber(sample.power.performance_freq_mhz, "MHz", 0));
    drawMetricLine(4, 42, "power", formatNumber(sample.power.cpu_power_watts, "W", 2));
    int cpu_content_y = 6;
    int cpu_content_h = std::max(1, top_h - cpu_content_y);
    int cpu_chart_h = cpu_content_h >= 7 ? std::clamp(top_h / 4, 2, 5) : 0;
    if (cpu_chart_h > 0) drawLineChart(cpu_content_y, 2, cpu_chart_h, std::max(10, left_w - 4), cpu_history, 2);
    int core_y = cpu_content_y + cpu_chart_h + (cpu_chart_h > 0 ? 1 : 0);
    drawCoreGrid(core_y, 2, std::max(1, top_h - core_y), std::max(10, left_w - 4), sample.core_usage_pct);

    drawBox(1, left_w, top_h, right_w, "GPU", 3);
    drawMetricLine(3, left_w + 2, "usage", formatNumber(sample.power.gpu_usage_pct, "%"));
    drawMetricLine(3, left_w + 22, "temp", formatNumber(sample.power.gpu_avg_temp_celsius, "C"));
    drawMetricLine(3, left_w + 39, "hot", formatNumber(sample.power.gpu_hot_temp_celsius, "C"));
    drawMetricLine(4, left_w + 2, "freq", formatNumber(sample.power.gpu_freq_mhz, "MHz", 0));
    drawMetricLine(4, left_w + 22, "power", formatNumber(sample.power.gpu_power_watts, "W", 2));
    int chart_w = std::max(10, right_w - 4);
    drawLineChart(6, left_w + 2, std::max(4, top_h - 8), chart_w, gpu_history, 3);

    drawBox(top_h + 1, 0, bottom_h, left_w, "Power & Battery", 4);
    drawMetricLine(top_h + 3, 2, "battery", formatNumber(sample.battery.capacity_pct, "%", 0));
    drawMetricLine(top_h + 3, 22, "state", sample.battery.is_charging ? "charging" : "battery/idle");
    drawMetricLine(top_h + 4, 2, "sys total", formatNumber(sample.power.system_power_watts, "W", 2));
    drawMetricLine(top_h + 4, 22, "dc in", formatNumber(sample.battery.input_power_watts, "W", 2));
    drawMetricLine(top_h + 5, 2, "battery out", formatNumber(sample.battery.battery_power_watts, "W", 2));
    drawMetricLine(top_h + 5, 22, "cycle", formatNumber(sample.battery.cycle_count, "", 0));
    double power_max = 20.0;
    for (double v : power_history) if (v > power_max) power_max = v;
    drawLineChartScaled(top_h + 6, 2, std::max(4, bottom_h - 8), std::max(10, left_w - 4), power_history, 4, power_max);

    drawBox(top_h + 1, left_w, bottom_h, right_w, "Memory", 5);
    drawMetricLine(top_h + 3, left_w + 2, "used", formatNumber(sample.memory.used_pct, "%"));
    drawText(top_h + 3, left_w + 20, formatBytes(sample.memory.used_bytes) + " / " + formatBytes(sample.memory.total_bytes), A_BOLD);
    drawMetricLine(top_h + 4, left_w + 2, "total", formatBytes(sample.memory.total_bytes));
    drawMetricLine(top_h + 4, left_w + 28, "avail", formatBytes(sample.memory.available_bytes));
    drawMetricLine(top_h + 5, left_w + 2, "cached", formatBytes(sample.memory.cached_bytes));
    drawMetricLine(top_h + 5, left_w + 28, "free", formatBytes(sample.memory.free_bytes));
    drawMetricLine(top_h + 6, left_w + 2, "swap", formatBytes(sample.memory.swap_used_bytes) + " / " + formatBytes(sample.memory.swap_total_bytes));
    drawLineChart(top_h + 8, left_w + 2, std::max(4, bottom_h - 10), std::max(10, right_w - 4), memory_history, 5);

    if (!sample.power.error.empty()) {
        drawText(h - 1, 2, sample.power.error + " | sudo ./pulse for powermetrics fields", A_DIM | COLOR_PAIR(6));
    }

    refresh();
}
