#include "ProcessUtils.hpp"

namespace ProcessUtils {

    Result getForegroundApplication(uint64_t *out_pid, uint64_t *out_title_id) {
        if (!out_pid || !out_title_id) {
            return MAKERESULT(Module_Libnx, LibnxError_BadInput);
        }

        // 1. Pega o PID do Application em foreground.
        uint64_t pid = 0;
        Result rc = pmdmntGetApplicationProcessId(&pid);
        if (R_FAILED(rc)) {
            return rc;
        }

        // 2. Resolve o TitleID a partir do PID.
        uint64_t tid = 0;
        rc = pminfoGetProgramId(&tid, pid);
        if (R_FAILED(rc)) {
            return rc;
        }

        *out_pid      = pid;
        *out_title_id = tid;
        return 0;
    }

    Result attachDebug(uint64_t pid, Handle *out_debug_handle) {
        if (!out_debug_handle) {
            return MAKERESULT(Module_Libnx, LibnxError_BadInput);
        }
        // svcDebugActiveProcess: kernel cria um handle para nos lermos a memoria
        // do processo alvo. Esse handle precisa ser fechado com svcCloseHandle
        // quando terminamos.
        return svcDebugActiveProcess(out_debug_handle, pid);
    }

    void detachDebug(Handle debug_handle) {
        if (debug_handle != INVALID_HANDLE) {
            svcCloseHandle(debug_handle);
        }
    }

} // namespace ProcessUtils
