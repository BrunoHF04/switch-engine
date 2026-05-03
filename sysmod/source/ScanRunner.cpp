#include "ScanRunner.hpp"

#include "Debugger.hpp"
#include "SysmodLog.hpp"
#include "seng_ipc.hpp"

#include "../../include/results_bin_format.hpp"

#include <sys/stat.h>

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

        Result resultsBegin(ResultsFileCtx *w) {
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
            seng::resultsbin::Header h{ seng::resultsbin::kMagic, seng::resultsbin::kVersion, 0 };
            std::fwrite(&h, sizeof(h), 1, w->fp);
            w->count  = 0;
            w->bufLen = 0;
            return 0;
        }

        void resultsPush(ResultsFileCtx *w, u64 addr, u32 val) {
            if (!w->fp) return;
            w->buf[w->bufLen++] = seng::resultsbin::Entry{ addr, val, 0 };
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
            seng::resultsbin::Header h{ seng::resultsbin::kMagic, seng::resultsbin::kVersion, w->count };
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
    } // namespace

    Result runFirstScanU32(uint64_t pid, uint32_t value, uint64_t *out_total_hits) {
        if (out_total_hits) *out_total_hits = 0;
        if (pid <= 1) {
            return MAKERESULT(Module_Libnx, LibnxError_BadInput);
        }

        Result rc = Debugger::attach(pid);
        if (R_FAILED(rc)) {
            seng::mod::log::write("[scan] attach pidLo=%u rc=0x%08X",
                                  static_cast<u32>(pid & 0xFFFFFFFFu), rc);
            return rc;
        }

        ResultsFileCtx writer{};
        rc = resultsBegin(&writer);
        if (R_FAILED(rc)) {
            Debugger::detach();
            return rc;
        }

        u64 addr = 0;
        while (true) {
            seng::MemoryRegion r{};
            rc = Debugger::queryMemory(addr, &r);
            if (R_FAILED(rc)) {
                seng::mod::log::write("[scan] queryMemory rc=0x%08X addrLo=%u",
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
                    if (got >= 4) {
                        const size_t end = got - 3;
                        for (size_t i = 0; i < end; i += 4) {
                            u32 curv = 0;
                            std::memcpy(&curv, s_scanBuf + i, sizeof(curv));
                            if (curv == value) {
                                resultsPush(&writer, cur + static_cast<u64>(i), curv);
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
        // Sempre detach apos scan: manter svcDebugActiveProcess aberto
        // congela o jogo por causa de debug events pendentes.
        // O poke re-attach + detach rapidamente quando precisar escrever.
        Debugger::detach();

        if (R_SUCCEEDED(rc) && out_total_hits) *out_total_hits = total;
        seng::mod::log::write("[scan] firstScan end rc=0x%08X hitsLo=%u",
                              rc, static_cast<u32>(total & 0xFFFFFFFFu));
        return rc;
    }

} // namespace seng::mod
