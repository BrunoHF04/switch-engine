#include "MemoryScanner.hpp"
#include "ProcessUtils.hpp"
#include "ResultsStore.hpp"
#include "SengClient.hpp"
#include "seng_ipc.hpp"

#include <cstring>

MemoryScanner::MemoryScanner()  = default;
MemoryScanner::~MemoryScanner() = default;

// =============================================================================
// Iteracao de regioes mapeadas (via IPC -> sysmod -> svcQueryDebugProcessMemory)
// =============================================================================
Result MemoryScanner::iterateRwRegions(Handle /*unused*/,
                                       std::function<Result(const MemoryInfo &)> cb) {
    uint64_t addr = 0;
    while (true) {
        seng::MemoryRegion r{};
        Result rc = SengClient::queryMemory(addr, &r);
        if (R_FAILED(rc)) return rc;

        // Adapta para a struct MemoryInfo de libnx (que o callback espera).
        MemoryInfo info{};
        info.addr = r.addr;
        info.size = r.size;
        info.type = r.type;
        info.attr = r.attr;
        info.perm = r.perm;

        const bool isReadWrite = (info.perm & Perm_R) && (info.perm & Perm_W);
        const bool isInteresting =
            info.type == MemType_Heap         ||
            info.type == MemType_CodeWritable ||
            info.type == MemType_AliasCode    ||
            info.type == MemType_Stack;

        if (isReadWrite && isInteresting && info.size > 0) {
            Result inner = cb(info);
            if (R_FAILED(inner)) return inner;
        }

        uint64_t next = info.addr + info.size;
        if (next <= addr) break; // wrap-around / fim
        addr = next;
    }
    return 0;
}

// =============================================================================
// Leitura em chunks (mantem RAM baixa). Cada chunk = 1 IPC ReadMemory.
// =============================================================================
Result MemoryScanner::readInChunks(Handle /*unused*/,
                                   uint64_t  addr,
                                   uint64_t  size,
                                   std::function<Result(uint64_t,
                                                        const uint8_t *,
                                                        size_t)> cb) {
    static thread_local uint8_t buffer[kScanBufferSize];

    uint64_t remaining = size;
    uint64_t cursor    = addr;

    while (remaining > 0) {
        const size_t chunk = (remaining < kScanBufferSize) ? remaining
                                                           : kScanBufferSize;
        size_t got = 0;
        Result rc  = SengClient::readMemory(cursor, buffer, chunk, &got);
        if (R_FAILED(rc) || got == 0) {
            // Pagina protegida ou erro: pula esse chunk e segue.
            cursor    += chunk;
            remaining -= chunk;
            continue;
        }

        rc = cb(cursor, buffer, got);
        if (R_FAILED(rc)) return rc;

        cursor    += got;
        remaining -= got;
    }
    return 0;
}

// =============================================================================
// First scan (uint32)
// =============================================================================
Result MemoryScanner::firstScanU32(uint32_t value) {
    if (m_targetPid == 0) return MAKERESULT(Module_Libnx, LibnxError_BadInput);

    Result rc = ProcessUtils::attachDebug(m_targetPid, nullptr);
    if (R_FAILED(rc)) return rc;

    ResultsStore::beginWrite();

    rc = iterateRwRegions(0, [&](const MemoryInfo &info) {
        return readInChunks(0, info.addr, info.size,
            [&](uint64_t base, const uint8_t *buf, size_t bufSize) {
                if (bufSize < 4) return 0;
                const size_t end = bufSize - 3;
                for (size_t i = 0; i < end; i += 4) {
                    uint32_t cur;
                    std::memcpy(&cur, buf + i, sizeof(cur));
                    if (cur == value) {
                        ResultsStore::pushHit(base + i, cur);
                    }
                }
                return 0;
            });
    });

    ResultsStore::endWrite();
    ProcessUtils::detachDebug(0);
    return rc;
}

// =============================================================================
// Next scan (uint32)
// =============================================================================
Result MemoryScanner::nextScanU32(uint32_t value) {
    if (m_targetPid == 0) return MAKERESULT(Module_Libnx, LibnxError_BadInput);

    Result rc = ProcessUtils::attachDebug(m_targetPid, nullptr);
    if (R_FAILED(rc)) return rc;

    rc = ResultsStore::filterU32([&](uint64_t addr,
                                     uint32_t /*oldVal*/,
                                     uint32_t *newVal) {
        uint32_t cur = 0;
        size_t   got = 0;
        Result   r   = SengClient::readMemory(addr, &cur, sizeof(cur), &got);
        if (R_FAILED(r) || got != sizeof(cur)) return false;
        *newVal = cur;
        return cur == value;
    });

    ProcessUtils::detachDebug(0);
    return rc;
}
