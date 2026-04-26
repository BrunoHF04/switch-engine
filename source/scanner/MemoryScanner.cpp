#include "MemoryScanner.hpp"
#include "ProcessUtils.hpp"
#include "ResultsStore.hpp"

#include <cstring>
#include <vector>

MemoryScanner::MemoryScanner()  = default;
MemoryScanner::~MemoryScanner() = default;

// =============================================================================
// Iteracao de regioes mapeadas
// =============================================================================
Result MemoryScanner::iterateRwRegions(Handle debugHandle,
                                       std::function<Result(const MemoryInfo &)> cb) {
    uint64_t addr = 0;
    while (true) {
        MemoryInfo  info{};
        u32         pageInfo = 0;

        Result rc = svcQueryDebugProcessMemory(&info, &pageInfo, debugHandle, addr);
        if (R_FAILED(rc)) {
            return rc;
        }

        const bool isReadWrite =
            (info.perm & Perm_R) && (info.perm & Perm_W);
        const bool isInteresting =
            info.type == MemType_Heap   ||
            info.type == MemType_CodeWritable ||
            info.type == MemType_AliasCode  ||
            info.type == MemType_Stack;

        if (isReadWrite && isInteresting && info.size > 0) {
            Result inner = cb(info);
            if (R_FAILED(inner)) {
                return inner;
            }
        }

        // Avanca. Quando addr + size dah overflow ou voltamos ao inicio, paramos.
        uint64_t next = info.addr + info.size;
        if (next <= addr) {
            break;
        }
        addr = next;
    }
    return 0;
}

// =============================================================================
// Leitura em chunks (mantem RAM baixa)
// =============================================================================
Result MemoryScanner::readInChunks(Handle      debugHandle,
                                   uint64_t    addr,
                                   uint64_t    size,
                                   std::function<Result(uint64_t,
                                                        const uint8_t *,
                                                        size_t)> cb) {
    static thread_local uint8_t buffer[kScanBufferSize];

    uint64_t remaining = size;
    uint64_t cursor    = addr;

    while (remaining > 0) {
        const size_t chunk = (remaining < kScanBufferSize) ? remaining
                                                           : kScanBufferSize;

        Result rc = svcReadDebugProcessMemory(buffer, debugHandle, cursor, chunk);
        if (R_FAILED(rc)) {
            // Algumas paginas podem estar guardadas; pulamos esse chunk.
            cursor    += chunk;
            remaining -= chunk;
            continue;
        }

        rc = cb(cursor, buffer, chunk);
        if (R_FAILED(rc)) {
            return rc;
        }

        cursor    += chunk;
        remaining -= chunk;
    }
    return 0;
}

// =============================================================================
// First scan (uint32)
// =============================================================================
Result MemoryScanner::firstScanU32(uint32_t value) {
    if (m_targetPid == 0) {
        return MAKERESULT(Module_Libnx, LibnxError_BadInput);
    }

    Handle debug = INVALID_HANDLE;
    Result rc = ProcessUtils::attachDebug(m_targetPid, &debug);
    if (R_FAILED(rc)) {
        return rc;
    }

    ResultsStore::beginWrite();

    rc = iterateRwRegions(debug, [&](const MemoryInfo &info) {
        return readInChunks(debug, info.addr, info.size,
            [&](uint64_t base, const uint8_t *buf, size_t bufSize) {
                // Compara em U32 alinhado a 4 bytes (mais rapido + menos lixo).
                const size_t end = (bufSize >= 4) ? bufSize - 3 : 0;
                for (size_t i = 0; i + 4 <= end + 3; i += 4) {
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
    ProcessUtils::detachDebug(debug);
    return rc;
}

// =============================================================================
// Next scan (uint32) - re-le os hits salvos e mantem so os que ainda batem
// =============================================================================
Result MemoryScanner::nextScanU32(uint32_t value) {
    if (m_targetPid == 0) {
        return MAKERESULT(Module_Libnx, LibnxError_BadInput);
    }

    Handle debug = INVALID_HANDLE;
    Result rc = ProcessUtils::attachDebug(m_targetPid, &debug);
    if (R_FAILED(rc)) {
        return rc;
    }

    rc = ResultsStore::filterU32([&](uint64_t addr, uint32_t /*oldVal*/, uint32_t *newVal) {
        uint32_t cur = 0;
        Result   r   = svcReadDebugProcessMemory(&cur, debug, addr, sizeof(cur));
        if (R_FAILED(r)) {
            return false;
        }
        *newVal = cur;
        return cur == value;
    });

    ProcessUtils::detachDebug(debug);
    return rc;
}
