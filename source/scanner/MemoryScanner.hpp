#pragma once

#include <switch.h>
#include <cstdint>
#include <functional>

#include "seng_ipc.hpp"

/**
 * MemoryScanner
 *
 * Motor de busca usando o sysmod via IPC. First scan delega para
 * StartMemoryScan (cmd 11) no sysmod; next scan faz read + filter local.
 *
 * Suporta todos os ValueType e CompareOp.
 */
class MemoryScanner {
public:
    MemoryScanner();
    ~MemoryScanner();

    void     setTargetPid(uint64_t pid) { m_targetPid = pid; }
    uint64_t getTargetPid() const       { return m_targetPid; }

    Result firstScan(seng::ValueType type, seng::CompareOp op,
                     uint64_t value, uint64_t value2 = 0);

    Result nextScan(seng::ValueType type, seng::CompareOp op,
                    uint64_t value, uint64_t value2 = 0);

    static constexpr size_t kScanBufferSize = 0x10000;

private:
    uint64_t m_targetPid = 0;
};
