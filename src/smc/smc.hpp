#pragma once

#include <IOKit/IOKitLib.h>

#include <cstdint>
#include <optional>

struct SmcVers {
    char major, minor, build, reserved;
    uint16_t release;
};

struct SmcPLimitData {
    uint16_t version, length;
    uint32_t cpuPLimit, gpuPLimit, memPLimit;
};

struct SmcKeyInfo {
    uint32_t dataSize;
    uint32_t dataType;
    char dataAttributes;
};

struct SmcParamStruct {
    uint32_t key;
    SmcVers vers;
    SmcPLimitData pLimitData;
    SmcKeyInfo keyInfo;
    char result;
    char status;
    char data8;
    uint32_t data32;
    uint8_t bytes[32];
};

uint32_t packSmcKey(const char* str);
bool openSmcConnection();
void closeSmcConnection();
std::optional<double> readSmcKey(uint32_t key);
std::optional<double> readSmcPower(const char* key);
