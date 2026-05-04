#include "Debugger.hpp"
#include "SysmodLog.hpp"

#include <switch/runtime/hosversion.h>
#include <switch/services/pgl.h>

#include <algorithm>
#include <mutex>
#include <cstring>

namespace Debugger {

    namespace {
        std::mutex g_mtx;
        Handle     g_debug = INVALID_HANDLE;
        uint64_t   g_pid   = 0;

        bool g_pmshell_ready = false;
        bool g_pgl_ready = false;

        void resolveProcessName(uint64_t pid, uint64_t tid, char *out, size_t outLen) {
            out[0] = '\0';
            if (tid == 0) {
                std::snprintf(out, outLen, "pid_%llu",
                              static_cast<unsigned long long>(pid));
                return;
            }
            // Tentar attach rapido para ler NACP nao e viavel em massa (lento e
            // arriscado). Usamos o TID para gerar um label legivel.
            if ((tid >> 56) == 0x01 ||
                (tid >= 0x0100000000000000ULL && tid < 0x0200000000000000ULL)) {
                std::snprintf(out, outLen, "App_%08X",
                              static_cast<uint32_t>(tid & 0xFFFFFFFFu));
            } else if (tid >= 0x0500000000000000ULL && tid < 0x0600000000000000ULL) {
                std::snprintf(out, outLen, "Sys_%08X",
                              static_cast<uint32_t>(tid & 0xFFFFFFFFu));
            } else if ((tid >> 60) == 0x4) {
                std::snprintf(out, outLen, "Hmb_%08X",
                              static_cast<uint32_t>(tid & 0xFFFFFFFFu));
            } else {
                std::snprintf(out, outLen, "Proc_%08X",
                              static_cast<uint32_t>(tid & 0xFFFFFFFFu));
            }
        }
    }

    void init() {
        if (hosversionAtLeast(10, 0, 0)) {
            Result prg = pglInitialize();
            if (R_SUCCEEDED(prg)) {
                g_pgl_ready = true;
                seng::mod::log::write("INFO [dbg] pglInitialize OK");
            } else {
                seng::mod::log::write("WARN [dbg] pglInitialize FAILED rc=0x%08X", prg);
            }
        }

        Result prc = pmshellInitialize();
        if (R_SUCCEEDED(prc)) {
            g_pmshell_ready = true;
            seng::mod::log::write("INFO [dbg] pmshellInitialize OK");
        } else {
            seng::mod::log::write("WARN [dbg] pmshellInitialize FAILED rc=0x%08X", prc);
        }
    }

    void shutdown() {
        detach();
    }

    void releaseAuxServicesForExit() {
        if (g_pgl_ready) {
            pglExit();
            g_pgl_ready = false;
        }
        if (g_pmshell_ready) {
            pmshellExit();
            g_pmshell_ready = false;
        }
    }

    Result attach(uint64_t pid) {
        std::lock_guard lock(g_mtx);
        if (g_debug != INVALID_HANDLE) {
            svcCloseHandle(g_debug);
            g_debug = INVALID_HANDLE;
            g_pid   = 0;
        }
        Handle h = INVALID_HANDLE;
        Result rc = svcDebugActiveProcess(&h, pid);
        if (R_FAILED(rc)) return rc;
        g_debug = h;
        g_pid   = pid;
        return 0;
    }

    void detach() {
        std::lock_guard lock(g_mtx);
        if (g_debug != INVALID_HANDLE) {
            svcCloseHandle(g_debug);
        }
        g_debug = INVALID_HANDLE;
        g_pid   = 0;
    }

    bool isAttached() {
        std::lock_guard lock(g_mtx);
        return g_debug != INVALID_HANDLE;
    }

    Result getForegroundPid(uint64_t *out_pid) {
        if (!out_pid) {
            return MAKERESULT(Module_Libnx, LibnxError_BadInput);
        }

        if (g_pgl_ready) {
            u64 pglPid = 0;
            Result rpg = pglGetApplicationProcessId(&pglPid);
            seng::mod::log::write(
                "INFO [dbg] getForegroundPid pgl rc=0x%08X pidLo=%u",
                rpg, static_cast<u32>(pglPid & 0xFFFFFFFFu));
            if (R_SUCCEEDED(rpg) && pglPid > 1) {
                *out_pid = pglPid;
                return 0;
            }
        }

        seng::mod::log::write("INFO [dbg] getForegroundPid: pmdmntGetApplicationProcessId");
        Result rc = pmdmntGetApplicationProcessId(out_pid);
        u64 pid = *out_pid;
        seng::mod::log::write(
            "INFO [dbg] getForegroundPid pmdmnt rc=0x%08X pidLo=%u",
            rc, static_cast<u32>(pid & 0xFFFFFFFFu));

        if (R_SUCCEEDED(rc) && pid > 1) {
            return rc;
        }

        if (g_pmshell_ready) {
            u64 shellPid = 0;
            Result rc2 = pmshellGetApplicationProcessIdForShell(&shellPid);
            seng::mod::log::write(
                "INFO [dbg] getForegroundPid pmshell fallback rc=0x%08X pidLo=%u",
                rc2, static_cast<u32>(shellPid & 0xFFFFFFFFu));
            if (R_SUCCEEDED(rc2) && shellPid > 1) {
                *out_pid = shellPid;
                return 0;
            }
        }

        return rc;
    }

    Result getTitleId(uint64_t pid, uint64_t *out_tid) {
        seng::mod::log::write("INFO [dbg] getTitleId pidLo=%u",
                              static_cast<u32>(pid & 0xFFFFFFFFu));
        Result rc = pminfoGetProgramId(out_tid, pid);
        u64 tid = out_tid ? *out_tid : 0;
        seng::mod::log::write(
            "INFO [dbg] getTitleId rc=0x%08X tidHi=%X tidLo=%X",
            rc,
            static_cast<u32>(tid >> 32),
            static_cast<u32>(tid & 0xFFFFFFFFu));
        return rc;
    }

    Result queryMemory(uint64_t addr, seng::MemoryRegion *out) {
        std::lock_guard lock(g_mtx);
        if (g_debug == INVALID_HANDLE) {
            return MAKERESULT(Module_Libnx, LibnxError_NotInitialized);
        }
        MemoryInfo info{};
        u32        page_info = 0;
        Result rc = svcQueryDebugProcessMemory(&info, &page_info, g_debug, addr);
        if (R_FAILED(rc)) return rc;

        out->addr      = info.addr;
        out->size      = info.size;
        out->type      = info.type;
        out->attr      = info.attr;
        out->perm      = info.perm;
        out->page_info = page_info;
        return 0;
    }

    Result readMemory(uint64_t addr, void *dst, size_t size) {
        std::lock_guard lock(g_mtx);
        if (g_debug == INVALID_HANDLE) {
            return MAKERESULT(Module_Libnx, LibnxError_NotInitialized);
        }
        return svcReadDebugProcessMemory(dst, g_debug, addr, size);
    }

    Result writeMemory(uint64_t addr, const void *src, size_t size) {
        std::lock_guard lock(g_mtx);
        if (g_debug == INVALID_HANDLE) {
            return MAKERESULT(Module_Libnx, LibnxError_NotInitialized);
        }
        return svcWriteDebugProcessMemory(g_debug, const_cast<void*>(src), addr, size);
    }

    Result listProcesses(seng::ProcessEntry *out,
                         size_t              max,
                         size_t             *out_count) {
        seng::mod::log::write("INFO [dbg] listProcesses begin max=%u",
                              static_cast<u32>(max));

        if (!out || !out_count || max == 0) {
            seng::mod::log::write("ERR  [dbg] listProcesses: BadInput");
            return MAKERESULT(Module_Libnx, LibnxError_BadInput);
        }
        *out_count = 0;

        u64 pids[seng::kMaxProcessList] = {};
        s32 num_out = 0;
        const s32 cap = static_cast<s32>(
            max < seng::kMaxProcessList ? max : seng::kMaxProcessList);

        Result rc = svcGetProcessList(&num_out, pids, static_cast<u32>(cap));
        seng::mod::log::write(
            "INFO [dbg] svcGetProcessList rc=0x%08X num_total=%d cap=%d",
            rc, num_out, cap);
        if (R_FAILED(rc)) return rc;

        const s32 filled = std::min(num_out, cap);

        size_t k = 0;
        for (s32 i = 0; i < filled && k < max; ++i) {
            if (pids[i] == 0) continue;

            u64 tid = 0;
            Result trc = pminfoGetProgramId(&tid, pids[i]);
            if (R_FAILED(trc)) tid = 0;

            out[k].pid = pids[i];
            out[k].tid = tid;
            resolveProcessName(pids[i], tid, out[k].name, sizeof(out[k].name));

            seng::mod::log::write("INFO [dbg]   [%d] pidLo=%u tidLo=%X name=%s",
                                  i,
                                  static_cast<u32>(pids[i] & 0xFFFFFFFFu),
                                  static_cast<u32>(tid & 0xFFFFFFFFu),
                                  out[k].name);
            ++k;
        }
        *out_count = k;
        seng::mod::log::write("INFO [dbg] listProcesses end k=%u",
                              static_cast<u32>(k));
        return 0;
    }

} // namespace Debugger
