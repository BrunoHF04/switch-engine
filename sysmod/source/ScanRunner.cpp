#include "ScanRunner.hpp"

#include "Debugger.hpp"
#include "SysmodLog.hpp"
#include "seng_ipc.hpp"

#include "../../include/results_bin_format.hpp"

#include <sys/stat.h>

#include <cmath>
#include <cstdio>
#include <cstring>

namespace seng::mod {

    namespace {
        constexpr size_t kScanBuf = 0x10000;
        static u8 s_scanBuf[kScanBuf];

        constexpr size_t kFlushEntries = 256;

        struct ResultsFileCtx {
            FILE              *fp      = nullptr;
            uint64_t           count   = 0;
            seng::resultsbin::Entry buf[kFlushEntries]{};
            size_t             bufLen = 0;
        };

        void flushResults(ResultsFileCtx *w) {
            if (!w->fp || w->bufLen == 0) return;
            std::fwrite(w->buf, sizeof(seng::resultsbin::Entry), w->bufLen, w->fp);
            w->bufLen = 0;
        }

        Result resultsBegin(ResultsFileCtx *w, ValueType type, CompareOp op) {
            if (w->fp) {
                std::fclose(w->fp);
                w->fp = nullptr;
            }
            mkdir("sdmc:/switch", 0777);
            mkdir("sdmc:/switch/switch-engine", 0777);
            w->fp = std::fopen(seng::resultsbin::kTempPath, "wb");
            if (!w->fp) {
                return MAKERESULT(Module_Libnx, LibnxError_NotFound);
            }
            seng::resultsbin::Header h{};
            h.magic      = seng::resultsbin::kMagic;
            h.version    = seng::resultsbin::kVersion;
            h.count      = 0;
            h.value_type = static_cast<uint8_t>(type);
            h.compare_op = static_cast<uint8_t>(op);
            std::fwrite(&h, sizeof(h), 1, w->fp);
            w->count  = 0;
            w->bufLen = 0;
            return 0;
        }

        void resultsPush(ResultsFileCtx *w, u64 addr, u64 rawVal) {
            if (!w->fp) return;
            w->buf[w->bufLen++] = seng::resultsbin::Entry{ addr, rawVal };
            w->count++;
            if (w->bufLen == kFlushEntries) {
                flushResults(w);
            }
        }

        Result resultsEnd(ResultsFileCtx *w, u64 *out_total) {
            if (!w->fp) {
                if (out_total) *out_total = 0;
                return MAKERESULT(Module_Libnx, LibnxError_NotInitialized);
            }
            flushResults(w);

            seng::resultsbin::Header h{};
            h.magic   = seng::resultsbin::kMagic;
            h.version = seng::resultsbin::kVersion;
            h.count   = w->count;
            std::rewind(w->fp);
            std::fwrite(&h, sizeof(h), 1, w->fp);
            std::fclose(w->fp);
            w->fp = nullptr;

            ::remove(seng::resultsbin::kResultsPath);
            if (::rename(seng::resultsbin::kTempPath, seng::resultsbin::kResultsPath) != 0) {
                if (out_total) *out_total = w->count;
                return MAKERESULT(Module_Libnx, LibnxError_NotFound);
            }
            if (out_total) *out_total = w->count;
            w->count = 0;
            return 0;
        }

        // -----------------------------------------------------------------
        // Comparador generico. Retorna true se o valor no buffer "passa".
        // -----------------------------------------------------------------
        uint64_t readRawFromBuf(const u8 *buf, size_t off, ValueType type) {
            uint64_t raw = 0;
            std::memcpy(&raw, buf + off, seng::valueTypeSize(type));
            return raw;
        }

        template <typename T>
        bool compareTyped(T cur, CompareOp op, T val, T val2) {
            switch (op) {
                case CompareOp::Equal:          return cur == val;
                case CompareOp::NotEqual:       return cur != val;
                case CompareOp::GreaterThan:    return cur > val;
                case CompareOp::LessThan:       return cur < val;
                case CompareOp::GreaterOrEqual: return cur >= val;
                case CompareOp::LessOrEqual:    return cur <= val;
                case CompareOp::Between:        return cur >= val && cur <= val2;
                case CompareOp::Unknown:        return true;
                default: return false;
            }
        }

        bool matchValue(uint64_t raw, ValueType type, CompareOp op,
                        uint64_t valRaw, uint64_t val2Raw) {
            switch (type) {
                case ValueType::U8: {
                    auto c  = static_cast<uint8_t>(raw);
                    auto v  = static_cast<uint8_t>(valRaw);
                    auto v2 = static_cast<uint8_t>(val2Raw);
                    return compareTyped(c, op, v, v2);
                }
                case ValueType::U16: {
                    auto c  = static_cast<uint16_t>(raw);
                    auto v  = static_cast<uint16_t>(valRaw);
                    auto v2 = static_cast<uint16_t>(val2Raw);
                    return compareTyped(c, op, v, v2);
                }
                case ValueType::U32: {
                    auto c  = static_cast<uint32_t>(raw);
                    auto v  = static_cast<uint32_t>(valRaw);
                    auto v2 = static_cast<uint32_t>(val2Raw);
                    return compareTyped(c, op, v, v2);
                }
                case ValueType::U64:
                    return compareTyped(raw, op, valRaw, val2Raw);
                case ValueType::F32: {
                    float c, v, v2;
                    uint32_t tmp;
                    tmp = static_cast<uint32_t>(raw);      std::memcpy(&c,  &tmp, 4);
                    tmp = static_cast<uint32_t>(valRaw);   std::memcpy(&v,  &tmp, 4);
                    tmp = static_cast<uint32_t>(val2Raw);  std::memcpy(&v2, &tmp, 4);
                    return compareTyped(c, op, v, v2);
                }
                case ValueType::F64: {
                    double c, v, v2;
                    std::memcpy(&c,  &raw,     8);
                    std::memcpy(&v,  &valRaw,  8);
                    std::memcpy(&v2, &val2Raw, 8);
                    return compareTyped(c, op, v, v2);
                }
            }
            return false;
        }
    } // namespace

    Result runFirstScan(uint64_t pid, const seng::ScanParams &params,
                        uint64_t *out_total_hits) {
        if (out_total_hits) *out_total_hits = 0;
        if (pid <= 1) {
            return MAKERESULT(Module_Libnx, LibnxError_BadInput);
        }

        const ValueType type = params.type;
        const CompareOp op   = params.op;
        const uint64_t  val  = params.value;
        const uint64_t  val2 = params.value2;
        const size_t    step = seng::valueTypeSize(type);

        Result rc = Debugger::attach(pid);
        if (R_FAILED(rc)) {
            seng::mod::log::write("ERR  [scan] attach pidLo=%u rc=0x%08X",
                                  static_cast<u32>(pid & 0xFFFFFFFFu), rc);
            return rc;
        }

        ResultsFileCtx writer{};
        rc = resultsBegin(&writer, type, op);
        if (R_FAILED(rc)) {
            Debugger::detach();
            return rc;
        }

        u64 addr = 0;
        while (true) {
            seng::MemoryRegion r{};
            rc = Debugger::queryMemory(addr, &r);
            if (R_FAILED(rc)) {
                seng::mod::log::write("WARN [scan] queryMemory rc=0x%08X addrLo=%u",
                                      rc, static_cast<u32>(addr & 0xFFFFFFFFu));
                break;
            }

            MemoryInfo info{};
            info.addr = r.addr;
            info.size = r.size;
            info.type = r.type;
            info.attr = r.attr;
            info.perm = r.perm;

            const bool rw = (info.perm & Perm_R) && (info.perm & Perm_W);
            const bool interesting =
                info.type == MemType_Heap               ||
                info.type == MemType_CodeMutable        ||
                info.type == MemType_ModuleCodeMutable  ||
                info.type == MemType_MappedMemory       ||
                info.type == MemType_WeirdMappedMem;

            if (rw && interesting && info.size > 0) {
                u64 rem = info.size;
                u64 cur = info.addr;
                while (rem > 0) {
                    const size_t chunk = (rem < kScanBuf) ? static_cast<size_t>(rem) : kScanBuf;
                    Result rr = Debugger::readMemory(cur, s_scanBuf, chunk);
                    const size_t got = R_SUCCEEDED(rr) ? chunk : 0;
                    if (got == 0) {
                        cur += chunk;
                        rem -= chunk;
                        continue;
                    }
                    if (got >= step) {
                        const size_t end = got - step + 1;
                        for (size_t i = 0; i < end; i += step) {
                            uint64_t rawCur = readRawFromBuf(s_scanBuf, i, type);
                            if (matchValue(rawCur, type, op, val, val2)) {
                                resultsPush(&writer, cur + static_cast<u64>(i), rawCur);
                            }
                        }
                    }
                    cur += got;
                    rem -= got;
                }
            }

            u64 next = info.addr + info.size;
            if (next <= addr) break;
            addr = next;
        }

        u64 total = 0;
        rc = resultsEnd(&writer, &total);
        Debugger::detach();

        if (R_SUCCEEDED(rc) && out_total_hits) *out_total_hits = total;
        seng::mod::log::write("INFO [scan] firstScan end rc=0x%08X hitsLo=%u type=%u op=%u",
                              rc, static_cast<u32>(total & 0xFFFFFFFFu),
                              static_cast<u32>(type), static_cast<u32>(op));
        return rc;
    }

} // namespace seng::mod
