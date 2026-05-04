#pragma once

#include <cstdint>
#include <cstddef>
#include <functional>

#include "seng_ipc.hpp"
#include "../../include/results_bin_format.hpp"

/**
 * ResultsStore
 *
 * Persistencia paginada de hits no SD card usando o formato definido em
 * results_bin_format.hpp (fonte unica de verdade para overlay e sysmod).
 *
 * O count() e' cacheado para evitar I/O a cada frame.
 */
namespace ResultsStore {

    using Header = seng::resultsbin::Header;
    using Entry  = seng::resultsbin::Entry;

    void   beginWrite(seng::ValueType type, seng::CompareOp op);
    void   pushHit(uint64_t address, uint64_t raw_value);
    void   endWrite();

    bool   filterTyped(seng::ValueType type, seng::CompareOp op,
                       uint64_t searchVal, uint64_t searchVal2,
                       std::function<bool(uint64_t addr,
                                          uint64_t oldRawVal,
                                          uint64_t *newRawVal)> readCurrent);

    void   readPage(size_t pageIndex, size_t pageSize,
                    std::function<void(const Entry &)> cb);

    size_t count();
    void   invalidateCache();
    void   reset();

    seng::ValueType currentValueType();
    seng::CompareOp currentCompareOp();

} // namespace ResultsStore
