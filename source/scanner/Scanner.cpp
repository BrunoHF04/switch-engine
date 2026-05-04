#include "Scanner.hpp"

#include "ProcessUtils.hpp"
#include "SengClient.hpp"
#include "seng_ipc.hpp"

namespace Scanner {

    Result detectForegroundAfterProcessList(uint64_t *out_pid, uint64_t *out_tid) {
        if (!out_pid || !out_tid) {
            return MAKERESULT(Module_Libnx, LibnxError_BadInput);
        }

        Result rc = SengClient::initialize();
        if (R_FAILED(rc)) return rc;

        // Verificacao de versao: garante compatibilidade overlay <-> sysmod.
        uint32_t sysmodVersion = 0;
        rc = SengClient::getVersion(&sysmodVersion);
        if (R_FAILED(rc)) return rc;

        return ProcessUtils::getForegroundApplication(out_pid, out_tid);
    }

} // namespace Scanner
