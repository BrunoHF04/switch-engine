#pragma once

#include <cstdint>

/**
 * Formato binario compartilhado (v2) entre overlay (ResultsStore) e sysmod
 * (ScanRunner) para sdmc:/switch/switch-engine/results.bin.
 *
 * Mudancas v1 -> v2:
 *   - Header ganha value_type e compare_op (24 bytes vs 16).
 *   - Entry::raw_value agora e uint64_t (suporta todos os tipos).
 *     Em v1 era {u32 value, u32 _pad=0}, que em little-endian e compativel
 *     quando lido como uint64_t.
 */
namespace seng::resultsbin {

    constexpr const char *kResultsPath = "sdmc:/switch/switch-engine/results.bin";
    constexpr const char *kTempPath    = "sdmc:/switch/switch-engine/results.tmp";
    constexpr uint32_t    kMagic       = 0x53454E47u; // 'SENG'
    constexpr uint32_t    kVersion     = 2;

    struct Header {
        uint32_t magic;
        uint32_t version;
        uint64_t count;
        uint8_t  value_type;   // seng::ValueType
        uint8_t  compare_op;   // seng::CompareOp
        uint8_t  _pad[6];
    };
    static_assert(sizeof(Header) == 24, "Header size mudou");

    struct Entry {
        uint64_t address;
        uint64_t raw_value;
    };
    static_assert(sizeof(Entry) == 16, "Entry size mudou");

} // namespace seng::resultsbin
