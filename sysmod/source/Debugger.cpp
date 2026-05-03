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

        /** pm:shell opcional — fallback quando pmdmnt devolve PID invalido. */
        bool g_pmshell_ready = false;

        /** pgl: PID do "application" real (HOS 10+); melhor com overlay em primeiro plano. */
        bool g_pgl_ready = false;
    }

    void init() {
        if (hosversionAtLeast(10, 0, 0)) {
            Result prg = pglInitialize();
            if (R_SUCCEEDED(prg)) {
                g_pgl_ready = true;
                seng::mod::log::writeRaw("[dbg] pglInitialize OK");
            } else {
                seng::mod::log::write("[dbg] pglInitialize FAILED rc=0x%08X", prg);
            }
        }

        Result prc = pmshellInitialize();
        if (R_SUCCEEDED(prc)) {
            g_pmshell_ready = true;
            seng::mod::log::writeRaw("[dbg] pmshellInitialize OK");
        } else {
            seng::mod::log::write("[dbg] pmshellInitialize FAILED rc=0x%08X", prc);
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

        // Com Tesla/Ultrahand aberto, pm:dmnt costuma reportar o applet/menu em
        // primeiro plano, nao o jogo. pglGetApplicationProcessId (10.0.0+)
        // aponta para o processo da aplicacao em execucao por baixo do overlay.
        if (g_pgl_ready) {
            u64 pglPid = 0;
            Result rpg = pglGetApplicationProcessId(&pglPid);
            seng::mod::log::write(
                "[dbg] getForegroundPid pgl rc=0x%08X pidLo=%u",
                rpg, static_cast<u32>(pglPid & 0xFFFFFFFFu));
            if (R_SUCCEEDED(rpg) && pglPid > 1) {
                *out_pid = pglPid;
                return 0;
            }
        }

        seng::mod::log::writeRaw("[dbg] getForegroundPid: pmdmntGetApplicationProcessId");
        Result rc = pmdmntGetApplicationProcessId(out_pid);
        u64 pid = *out_pid;
        seng::mod::log::write("[dbg] getForegroundPid pmdmnt rc=0x%08X pidLo=%u",
                              rc, static_cast<u32>(pid & 0xFFFFFFFFu));

        // PID 0 invalido; PID 1 costuma ser kernel — nao e' um app de jogo.
        if (R_SUCCEEDED(rc) && pid > 1) {
            return rc;
        }

        if (g_pmshell_ready) {
            u64 shellPid = 0;
            Result rc2 = pmshellGetApplicationProcessIdForShell(&shellPid);
            seng::mod::log::write(
                "[dbg] getForegroundPid pmshell fallback rc=0x%08X pidLo=%u",
                rc2, static_cast<u32>(shellPid & 0xFFFFFFFFu));
            if (R_SUCCEEDED(rc2) && shellPid > 1) {
                *out_pid = shellPid;
                return 0;
            }
        }

        return rc;
    }

    Result getTitleId(uint64_t pid, uint64_t *out_tid) {
        seng::mod::log::write("[dbg] getTitleId pidLo=%u: calling pminfoGetProgramId",
                              static_cast<u32>(pid & 0xFFFFFFFFu));
        Result rc = pminfoGetProgramId(out_tid, pid);
        u64 tid = out_tid ? *out_tid : 0;
        seng::mod::log::write("[dbg] getTitleId rc=0x%08X tidHi=%X tidLo=%X",
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
        // svcWriteDebugProcessMemory pede void* nao-const por API; respeitamos.
        return svcWriteDebugProcessMemory(g_debug, const_cast<void*>(src), addr, size);
    }

    Result listProcesses(seng::ProcessEntry *out,
                         size_t              max,
                         size_t             *out_count) {
        seng::mod::log::write("[dbg] listProcesses begin max=%u outNull=%u outCntNull=%u",
                              static_cast<u32>(max),
                              out == nullptr ? 1u : 0u,
                              out_count == nullptr ? 1u : 0u);

        if (!out || !out_count || max == 0) {
            seng::mod::log::writeRaw("[dbg] listProcesses: BadInput");
            return MAKERESULT(Module_Libnx, LibnxError_BadInput);
        }
        *out_count = 0;

        // svcGetProcessList preenche um array de PIDs e retorna a contagem
        // real preenchida em num_out. Cap em seng::kMaxProcessList por
        // seguranca (e' o que a IPC entrega ao cliente de qualquer forma).
        u64 pids[seng::kMaxProcessList] = {};
        s32 num_out = 0;
        const s32 cap = static_cast<s32>(
            max < seng::kMaxProcessList ? max : seng::kMaxProcessList);

        seng::mod::log::write("[dbg] listProcesses: svcGetProcessList cap=%d",
                              cap);
        Result rc = svcGetProcessList(&num_out, pids, static_cast<u32>(cap));
        seng::mod::log::write(
            "[dbg] listProcesses: svcGetProcessList rc=0x%08X num_total=%d cap=%d",
            rc, num_out, cap);
        if (R_FAILED(rc)) return rc;

        // NumProcesses (num_out) e' o TOTAL de processos vivos no sistema, nao
        // o tamanho preenchido no buffer. Se num_out > cap, so' as primeiras
        // `cap` entradas de `pids` sao validas — iterar ate num_out lixo fora
        // do buffer (PID 0 fantasma, lista vazia, etc.). Ver switchbrew SVC.
        const s32 filled = std::min(num_out, cap);

        size_t k = 0;
        for (s32 i = 0; i < filled && k < max; ++i) {
            if (pids[i] == 0) {
                continue;
            }
            u64 tid = 0;
            // pminfoGetProgramId so' funciona para processos com program id
            // registrado (apps/sysmodulos com NPDM). Pra kernel/init etc
            // ignora (tid=0). Nao queremos abortar a lista por causa disso.
            Result trc = pminfoGetProgramId(&tid, pids[i]);
            if (R_FAILED(trc)) tid = 0;

            seng::mod::log::write("[dbg]   [%d] pidLo=%u tidLo=%X trc=0x%08X",
                                  i,
                                  static_cast<u32>(pids[i] & 0xFFFFFFFFu),
                                  static_cast<u32>(tid & 0xFFFFFFFFu),
                                  trc);

            out[k].pid = pids[i];
            out[k].tid = tid;
            ++k;
        }
        *out_count = k;
        seng::mod::log::write("[dbg] listProcesses end k=%u",
                              static_cast<u32>(k));
        return 0;
    }

} // namespace Debugger
