#include "Debugger.hpp"

#include <mutex>
#include <cstring>

namespace Debugger {

    namespace {
        std::mutex g_mtx;
        Handle     g_debug = INVALID_HANDLE;
        uint64_t   g_pid   = 0;
    }

    void init() {
        // Nada que precise de inicializacao ativa por enquanto.
    }

    void shutdown() {
        detach();
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
        return pmdmntGetApplicationProcessId(out_pid);
    }

    Result getTitleId(uint64_t pid, uint64_t *out_tid) {
        return pminfoGetProgramId(out_tid, pid);
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

} // namespace Debugger
