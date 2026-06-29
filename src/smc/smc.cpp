#include "smc.hpp"

#include <cstring>
#include <string>

#include "../core/constants.hpp"

static io_connect_t g_smc_connection = 0;

uint32_t packSmcKey(const char* str) {
    return (static_cast<uint32_t>(static_cast<unsigned char>(str[0])) << 24) |
           (static_cast<uint32_t>(static_cast<unsigned char>(str[1])) << 16) |
           (static_cast<uint32_t>(static_cast<unsigned char>(str[2])) << 8) |
           static_cast<uint32_t>(static_cast<unsigned char>(str[3]));
}

bool openSmcConnection() {
    if (g_smc_connection) return true;
    io_iterator_t iter;
    if (IOServiceGetMatchingServices(kIOMainPortDefault, IOServiceMatching("AppleSMC"), &iter) != KERN_SUCCESS)
        return false;
    io_object_t device;
    bool opened = false;
    while ((device = IOIteratorNext(iter)) != 0) {
        char name[128] = {};
        IORegistryEntryGetName(device, name);
        if (std::string(name) == "AppleSMCKeysEndpoint") {
            opened = IOServiceOpen(device, mach_task_self(), 0, &g_smc_connection) == KERN_SUCCESS;
        }
        IOObjectRelease(device);
        if (opened) break;
    }
    IOObjectRelease(iter);
    return opened;
}

void closeSmcConnection() {
    if (g_smc_connection) {
        IOServiceClose(g_smc_connection);
        g_smc_connection = 0;
    }
}

std::optional<double> readSmcKey(uint32_t key) {
    SmcParamStruct input{}, output{};
    input.key = key;
    input.data8 = static_cast<char>(SMC_CMD_READ_KEY_INFO);

    output = {};
    size_t out_size = sizeof(output);
    kern_return_t kr = IOConnectCallStructMethod(g_smc_connection, SMC_KERNEL_INDEX,
                                                  &input, sizeof(input), &output, &out_size);
    if (kr != KERN_SUCCESS || output.result != 0) return std::nullopt;

    uint32_t data_size = output.keyInfo.dataSize;
    uint32_t data_type = output.keyInfo.dataType;

    input.keyInfo.dataSize = data_size;
    input.data8 = static_cast<char>(SMC_CMD_READ_BYTES);
    output = {};
    out_size = sizeof(output);
    kr = IOConnectCallStructMethod(g_smc_connection, SMC_KERNEL_INDEX,
                                    &input, sizeof(input), &output, &out_size);
    if (kr != KERN_SUCCESS || output.result != 0) return std::nullopt;

    if (data_type == 0x666C7420 && data_size == 4) {
        float val;
        std::memcpy(&val, output.bytes, 4);
        return static_cast<double>(val);
    }
    if (data_type == 0x73703738 && data_size == 2) {
        return static_cast<double>(static_cast<int8_t>(output.bytes[0])) +
               static_cast<double>(output.bytes[1]) / 256.0;
    }
    if (data_type == 0x66706532 && data_size == 2) {
        uint16_t raw = (static_cast<uint16_t>(output.bytes[0]) << 8) | output.bytes[1];
        return static_cast<double>(raw) / 4.0;
    }
    if (data_type == 0x75693332 && data_size == 4) {
        uint32_t raw;
        std::memcpy(&raw, output.bytes, 4);
        return static_cast<double>(raw);
    }
    if (data_type == 0x75693136 && data_size == 2) {
        uint16_t raw = (static_cast<uint16_t>(output.bytes[0]) << 8) | output.bytes[1];
        return static_cast<double>(raw);
    }
    return std::nullopt;
}

std::optional<double> readSmcPower(const char* key) {
    return readSmcKey(packSmcKey(key));
}
