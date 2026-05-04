#include "MemoryScanner.hpp"
#include "ProcessUtils.hpp"
#include "ResultsStore.hpp"
#include "SengClient.hpp"
#include "seng_ipc.hpp"

#include <cstring>

MemoryScanner::MemoryScanner()  = default;
MemoryScanner::~MemoryScanner() = default;

Result MemoryScanner::firstScan(seng::ValueType type, seng::CompareOp op,
                                uint64_t value, uint64_t value2) {
    if (m_targetPid == 0) return MAKERESULT(Module_Libnx, LibnxError_BadInput);

    Result rc = SengClient::initialize();
    if (R_FAILED(rc)) return rc;

    seng::ScanParams params{};
    params.pid    = m_targetPid;
    params.value  = value;
    params.value2 = value2;
    params.type   = type;
    params.op     = op;

    u64 hits = 0;
    rc = SengClient::startMemoryScan(params, &hits);
    if (R_SUCCEEDED(rc)) {
        ResultsStore::invalidateCache();
    }
    return rc;
}

namespace {
    bool matchValueOverlay(uint64_t raw, seng::ValueType type, seng::CompareOp op,
                           uint64_t valRaw, uint64_t val2Raw) {
        // Reuso da logica de comparacao — versao simplificada overlay-side.
        // Changed/Unchanged sao resolvidos pelo caller (compara old vs new).
        switch (type) {
            case seng::ValueType::U8:
                return seng::CompareOp::Equal == op
                    ? static_cast<uint8_t>(raw) == static_cast<uint8_t>(valRaw)
                    : true;
            case seng::ValueType::U16:
                return seng::CompareOp::Equal == op
                    ? static_cast<uint16_t>(raw) == static_cast<uint16_t>(valRaw)
                    : true;
            case seng::ValueType::U32:
                return seng::CompareOp::Equal == op
                    ? static_cast<uint32_t>(raw) == static_cast<uint32_t>(valRaw)
                    : true;
            case seng::ValueType::U64:
                return op == seng::CompareOp::Equal ? raw == valRaw : true;
            default:
                return true;
        }
    }
}

Result MemoryScanner::nextScan(seng::ValueType type, seng::CompareOp op,
                               uint64_t value, uint64_t value2) {
    if (m_targetPid == 0) return MAKERESULT(Module_Libnx, LibnxError_BadInput);

    Result rc = ProcessUtils::attachDebug(m_targetPid, nullptr);
    if (R_FAILED(rc)) return rc;

    const size_t valSize = seng::valueTypeSize(type);

    bool ok = ResultsStore::filterTyped(type, op, value, value2,
        [&](uint64_t addr, uint64_t oldRaw, uint64_t *newRaw) -> bool {
            uint64_t cur = 0;
            size_t   got = 0;
            Result   r   = SengClient::readMemory(addr, &cur, valSize, &got);
            if (R_FAILED(r) || got != valSize) return false;
            *newRaw = cur;

            switch (op) {
                case seng::CompareOp::Equal:
                    return cur == value;
                case seng::CompareOp::NotEqual:
                    return cur != value;
                case seng::CompareOp::GreaterThan:
                    return cur > value;
                case seng::CompareOp::LessThan:
                    return cur < value;
                case seng::CompareOp::GreaterOrEqual:
                    return cur >= value;
                case seng::CompareOp::LessOrEqual:
                    return cur <= value;
                case seng::CompareOp::Between:
                    return cur >= value && cur <= value2;
                case seng::CompareOp::Changed:
                    return cur != oldRaw;
                case seng::CompareOp::Unchanged:
                    return cur == oldRaw;
                case seng::CompareOp::Unknown:
                    return true;
            }
            return false;
        });

    ProcessUtils::detachDebug(0);
    return ok ? 0 : MAKERESULT(Module_Libnx, LibnxError_NotFound);
}
