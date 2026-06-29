# pulse

A macOS C++ terminal performance monitor (TUI).

The project uses ncurses for real-time UI rendering and collects metrics through system interfaces such as IOKit, CoreFoundation, SMC, HID, Mach, and sysctl/vm_statistics. When available, it also merges data from powermetrics and IOReport.

## Features

- CPU: per-core usage, average usage, average temperature, hotspot temperature, E/P core frequency, and CPU power
- GPU: usage, average temperature, hotspot temperature, frequency, and GPU power
- Power & battery: battery level, total system power, adapter input power, and battery charge/discharge power
- Memory: used/available/cached/free memory and swap usage
- UI: refreshes every 1000ms with in-place redraw (no terminal flooding)

## Requirements

- macOS (optimized for Apple Silicon scenarios)
- CMake >= 3.20
- clang++ (with C++20 support)
- ncurses

Notes:
- This project is designed and validated on macOS only.
- powermetrics-related fields usually require root privileges.

## Quick Start

### 1) Configure and build

```sh
cmake -B build
cmake --build build
```

Binary output:

```sh
build/pulse
```

### 2) Run

Run normally (most base metrics are available):

```sh
./build/pulse
```

Run with sudo (recommended for richer power/frequency/temperature fields):

```sh
sudo ./build/pulse
```

Exit: press `q`

## Data Sources and Priority

For overlapping metrics, the program merges values based on availability and reliability. Core sources:

1. IOReport private framework (runtime `dlopen`)
2. powermetrics (periodic sampling in a background thread)
3. SMC keys
4. HID temperatures
5. Mach per-core CPU usage
6. sysctl / vm_statistics64 (memory and swap)
7. IOPowerSources + AppleSmartBattery (battery)

When higher-priority sources are unavailable, it automatically falls back to other sources.

## UI Layout

- Top-left: CPU panel (metrics + history chart + core grid)
- Top-right: GPU panel (metrics + history chart)
- Bottom-left: Power & Battery (power metrics + power history)
- Bottom-right: Memory (memory metrics + history chart)

Minimum terminal size:

- 64 columns x 18 rows

If the terminal is smaller than this, a warning message is shown.

## Common Behavior and Known Limitations

- Without sudo, powermetrics may fail with permission errors, and related fields show `N/A`.
- On some macOS versions, CPU power may be unavailable; the program safely shows `N/A`.
- IOReport is a private framework, so OS differences may cause some fields to be missing.

## Debug and Logs

The program writes some debug files to the temp directory:

- `/tmp/pulse_debug.log`: temperature collection debug information
- `/tmp/pulse_powermetrics.txt`: raw powermetrics output

## Project Structure

```text
src/
	main.cpp                 # entry point and sample aggregation
	core/                    # types and constants
	utils/                   # formatting and shared helpers
	smc/                     # SMC access
	io_report/               # IOReport dynamic loading and power/frequency reads
	sensors/                 # temperature/CPU/memory/battery collection
	powermetrics/            # powermetrics invocation and parsing
	ui/                      # ncurses rendering
```

## Development Notes

- C++ standard: C++20
- Compiler flags: `-Wall -Wextra -Wpedantic -O2`
- Linked libraries: IOKit, CoreFoundation, ncurses

This repository currently has no unit tests or CI. After each change, at minimum:

1. Rebuild
2. Run locally and check that TUI rendering is correct
3. Verify field behavior with and without sudo
