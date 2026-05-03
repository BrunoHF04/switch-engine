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

        seng::ProcessEntry entries[seng::kMaxProcessList]{};
        size_t             n = 0;
        rc = SengClient::listProcesses(entries, seng::kMaxProcessList, &n);
        if (R_FAILED(rc)) return rc;

        return ProcessUtils::getForegroundApplication(out_pid, out_tid);
    }

} // namespace Scanner
