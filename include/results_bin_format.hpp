#pragma once

#include <cstdint>

/**
 * Formato binario compartilhado entre overlay (ResultsStore) e sysmod
 * (ResultsBinWriter) para sdmc:/switch/switch-engine/results.bin
 */
namespace seng::resultsbin {

    constexpr const char *kResultsPath = "sdmc:/switch/switch-engine/results.bin";
    constexpr const char *kTempPath    = "sdmc:/switch/switch-engine/results.tmp";
    constexpr uint32_t    kMagic       = 0x53454E47u; // 'SENG'
    constexpr uint32_t    kVersion     = 1;

    struct Header {
        uint32_t magic;
        uint32_t version;
        uint64_t count;
    };

    struct Entry {
        uint64_t address;
        uint32_t value;
        uint32_t _pad;
    };

} // namespace seng::resultsbin
